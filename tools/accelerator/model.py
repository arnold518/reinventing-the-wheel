#!/usr/bin/env python3
"""Train, quantize and export the chapter 6 networks (MNIST, numpy only).

    python3 tools/accelerator/model.py [--data DIR]

Downloads MNIST into DIR (default build-accel/mnist, not tracked), trains a
784 -> 64 -> 10 MLP and a small CNN (3x3 conv, 8 filters, stride 2 -> 3x3 conv,
16 filters, stride 2 -> 576 -> 10), quantizes both to int8 with int32 sums and
writes:

  tools/accelerator/guest/model_cpu.h        weights in natural layout (CPU programs)
  tools/accelerator/guest/model_tiled_nN.h   weights in the device's tile layout, N = 4, 8, 16
  tools/accelerator/guest/images.h           test images and the expected int8 outputs
  tests/fixtures/accelerator/model.json      scales, accuracies and expected outputs

The integer inference here follows the device bit for bit: int32 sums, then
out = clamp((acc * scale + 2^(shift-1)) >> shift, -128, 127), then ReLU.
Everything is seeded, so a rerun gives the same files.
"""
import argparse
import gzip
import json
import os
import struct
import urllib.request

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GUEST = os.path.join(ROOT, "tools", "accelerator", "guest")
FIXTURES = os.path.join(ROOT, "tests", "fixtures", "accelerator")
MIRROR = "https://storage.googleapis.com/cvdf-datasets/mnist/"
FILES = {"train_x": "train-images-idx3-ubyte.gz", "train_y": "train-labels-idx1-ubyte.gz",
         "test_x": "t10k-images-idx3-ubyte.gz", "test_y": "t10k-labels-idx1-ubyte.gz"}
IMAGES = 8            # test images embedded in the guest programs
SIZES = (4, 8, 16)


def load(data_dir):
    os.makedirs(data_dir, exist_ok=True)
    out = {}
    for key, name in FILES.items():
        path = os.path.join(data_dir, name)
        if not os.path.exists(path):
            urllib.request.urlretrieve(MIRROR + name, path)
        with gzip.open(path, "rb") as f:
            raw = f.read()
        if key.endswith("_x"):
            n, rows, cols = struct.unpack(">III", raw[4:16])
            out[key] = np.frombuffer(raw[16:], np.uint8).reshape(n, rows, cols)
        else:
            out[key] = np.frombuffer(raw[8:], np.uint8).astype(np.int64)
    return out


# ---------------------------------------------------------------- training
def im2col(x, stride):
    """x: (B, H, W, C) -> (B, OH, OW, 9*C) in (ky, kx, c) order."""
    b, h, w, c = x.shape
    oh, ow = (h - 3) // stride + 1, (w - 3) // stride + 1
    cols = np.empty((b, oh, ow, 9 * c), x.dtype)
    for ky in range(3):
        for kx in range(3):
            patch = x[:, ky:ky + stride * (oh - 1) + 1:stride, kx:kx + stride * (ow - 1) + 1:stride, :]
            cols[..., (ky * 3 + kx) * c:(ky * 3 + kx + 1) * c] = patch
    return cols


def col2im(dcols, shape, stride):
    b, h, w, c = shape
    oh, ow = dcols.shape[1], dcols.shape[2]
    dx = np.zeros(shape, dcols.dtype)
    for ky in range(3):
        for kx in range(3):
            dx[:, ky:ky + stride * (oh - 1) + 1:stride, kx:kx + stride * (ow - 1) + 1:stride, :] += \
                dcols[..., (ky * 3 + kx) * c:(ky * 3 + kx + 1) * c]
    return dx


class Adam:
    def __init__(self, params, lr=1e-3):
        self.params, self.lr, self.t = params, lr, 0
        self.m = [np.zeros_like(p) for p in params]
        self.v = [np.zeros_like(p) for p in params]

    def step(self, grads):
        self.t += 1
        for p, g, m, v in zip(self.params, grads, self.m, self.v):
            m *= 0.9; m += 0.1 * g
            v *= 0.999; v += 0.001 * g * g
            p -= self.lr * (m / (1 - 0.9 ** self.t)) / (np.sqrt(v / (1 - 0.999 ** self.t)) + 1e-8)


def softmax_grad(logits, labels):
    z = logits - logits.max(axis=1, keepdims=True)
    p = np.exp(z); p /= p.sum(axis=1, keepdims=True)
    p[np.arange(len(labels)), labels] -= 1
    return p / len(labels)


def train_mlp(x, y, rng, epochs=6):
    w1 = rng.normal(0, np.sqrt(2 / 784), (784, 64)).astype(np.float32); b1 = np.zeros(64, np.float32)
    w2 = rng.normal(0, np.sqrt(2 / 64), (64, 10)).astype(np.float32); b2 = np.zeros(10, np.float32)
    opt = Adam([w1, b1, w2, b2])
    for _ in range(epochs):
        order = rng.permutation(len(x))
        for i in range(0, len(x), 64):
            idx = order[i:i + 64]; xb = x[idx]
            h = np.maximum(xb @ w1 + b1, 0)
            g = softmax_grad(h @ w2 + b2, y[idx])
            gh = (g @ w2.T) * (h > 0)
            opt.step([xb.T @ gh, gh.sum(0), h.T @ g, g.sum(0)])
    return {"w1": w1, "b1": b1, "w2": w2, "b2": b2}


def train_cnn(x, y, rng, epochs=3):
    c1 = rng.normal(0, np.sqrt(2 / 9), (9, 8)).astype(np.float32); d1 = np.zeros(8, np.float32)
    c2 = rng.normal(0, np.sqrt(2 / 72), (72, 16)).astype(np.float32); d2 = np.zeros(16, np.float32)
    f = rng.normal(0, np.sqrt(2 / 576), (576, 10)).astype(np.float32); e = np.zeros(10, np.float32)
    opt = Adam([c1, d1, c2, d2, f, e])
    for _ in range(epochs):
        order = rng.permutation(len(x))
        for i in range(0, len(x), 64):
            idx = order[i:i + 64]; xb = x[idx].reshape(-1, 28, 28, 1)
            p1 = im2col(xb, 2); a1 = np.maximum(p1 @ c1 + d1, 0)            # (B,13,13,8)
            p2 = im2col(a1, 2); a2 = np.maximum(p2 @ c2 + d2, 0)            # (B,6,6,16)
            flat = a2.reshape(len(idx), -1)
            g = softmax_grad(flat @ f + e, y[idx])
            gflat = (g @ f.T).reshape(a2.shape) * (a2 > 0)
            gc2 = p2.reshape(-1, 72).T @ gflat.reshape(-1, 16)
            ga1 = col2im(gflat @ c2.T, a1.shape, 2) * (a1 > 0)
            gc1 = p1.reshape(-1, 9).T @ ga1.reshape(-1, 8)
            opt.step([gc1, ga1.sum((0, 1, 2)), gc2, gflat.sum((0, 1, 2)), flat.T @ g, g.sum(0)])
    return {"c1": c1, "d1": d1, "c2": c2, "d2": d2, "f": f, "e": e}


# ---------------------------------------------------------------- quantization
def requantize(acc, scale, shift, relu):
    acc = np.asarray(acc, np.int64)
    acc = ((acc + 2**31) % 2**32) - 2**31          # int32 wrap, as the device sums
    v = acc * scale
    if shift:
        v = v + (1 << (shift - 1))
    v = v >> shift
    v = np.clip(v, -128, 127)
    if relu:
        v = np.maximum(v, 0)
    return v.astype(np.int8)


def multiplier(m):
    """Scale and shift with scale / 2^shift ~ m, scale < 2^16."""
    shift = int(np.floor(np.log2(65535 / m)))
    shift = max(0, min(31, shift))
    scale = int(round(m * 2**shift))
    assert 0 < scale < 65536, (m, scale, shift)
    return scale, shift


def quantize_layer(w, b, s_x, z_cal, relu):
    """w: (K, O) float. Returns int8 weights, int32 bias, scale, shift, output scale."""
    s_w = float(np.abs(w).max()) / 127
    wq = np.clip(np.round(w / s_w), -127, 127).astype(np.int8)
    bq = np.round(b / (s_x * s_w)).astype(np.int64)
    top = float(np.percentile(np.maximum(z_cal, 0) if relu else np.abs(z_cal), 99.99))
    s_y = max(top, 1e-6) / 127
    scale, shift = multiplier(s_x * s_w / s_y)
    return wq, bq.astype(np.int32), scale, shift, s_y


def quantize_input(images):
    return np.round(images.astype(np.float64) * 127 / 255).astype(np.int8)


def mlp_int(q, xq):
    h = requantize(xq.astype(np.int64) @ q["w1"].astype(np.int64) + q["b1"], q["scale1"], q["shift1"], True)
    return requantize(h.astype(np.int64) @ q["w2"].astype(np.int64) + q["b2"], q["scale2"], q["shift2"], False)


def conv_int(xq, wq, bq, scale, shift, relu, stride):
    cols = im2col(xq.astype(np.int64), stride)
    return requantize(cols @ wq.astype(np.int64) + bq, scale, shift, relu)


def cnn_int(q, xq):
    a = conv_int(xq.reshape(-1, 28, 28, 1), q["c1"], q["d1"], q["s1"], q["h1"], True, 2)
    a = conv_int(a, q["c2"], q["d2"], q["s2"], q["h2"], True, 2)
    return requantize(a.reshape(len(xq), -1).astype(np.int64) @ q["f"].astype(np.int64) + q["e"], q["s3"], q["h3"], False)


def quantize_mlp(m, x_cal):
    s0 = 1 / 127
    z1 = x_cal @ m["w1"] + m["b1"]
    w1, b1, sc1, sh1, s1 = quantize_layer(m["w1"], m["b1"], s0, z1, True)
    z2 = np.maximum(z1, 0) @ m["w2"] + m["b2"]
    w2, b2, sc2, sh2, _ = quantize_layer(m["w2"], m["b2"], s1, z2, False)
    return {"w1": w1, "b1": b1, "scale1": sc1, "shift1": sh1, "w2": w2, "b2": b2, "scale2": sc2, "shift2": sh2}


def quantize_cnn(m, x_cal):
    s0 = 1 / 127
    xb = x_cal.reshape(-1, 28, 28, 1)
    z1 = im2col(xb, 2) @ m["c1"] + m["d1"]
    c1, d1, sc1, sh1, s1 = quantize_layer(m["c1"], m["d1"], s0, z1, True)
    a1 = np.maximum(z1, 0)
    z2 = im2col(a1, 2) @ m["c2"] + m["d2"]
    c2, d2, sc2, sh2, s2 = quantize_layer(m["c2"], m["d2"], s1, z2, True)
    z3 = np.maximum(z2, 0).reshape(len(x_cal), -1) @ m["f"] + m["e"]
    f, e, sc3, sh3, _ = quantize_layer(m["f"], m["e"], s2, z3, False)
    return {"c1": c1, "d1": d1, "s1": sc1, "h1": sh1, "c2": c2, "d2": d2, "s2": sc2, "h2": sh2,
            "f": f, "e": e, "s3": sc3, "h3": sh3}


# ---------------------------------------------------------------- export
def tiles(w, n):
    """(K, O) int8 -> bytes in the device's tile order [tile][chunk][row][column]."""
    k, o = w.shape
    chunks, ntiles = -(-k // n), -(-o // n)
    out = np.zeros((ntiles, chunks, n, n), np.int8)
    padded = np.zeros((chunks * n, ntiles * n), np.int8)
    padded[:k, :o] = w
    for t in range(ntiles):
        for c in range(chunks):
            out[t, c] = padded[c * n:(c + 1) * n, t * n:(t + 1) * n]
    return out.reshape(-1)


def padded_bias(b, n):
    out = np.zeros(-(-len(b) // n) * n, np.int32)
    out[:len(b)] = b
    return out


def c_array(name, values, ctype, per_line=24):
    values = [int(v) for v in np.asarray(values).reshape(-1)]
    lines = []
    for i in range(0, len(values), per_line):
        lines.append("  " + ",".join(str(v) for v in values[i:i + per_line]) + ",")
    align = " __attribute__((aligned(4)))"
    return f"static const {ctype} {name}[{len(values)}]{align} = {{\n" + "\n".join(lines) + "\n};\n"


def header(guard, body):
    return (f"/* Generated by tools/accelerator/model.py from MNIST; do not edit. */\n"
            f"#ifndef {guard}\n#define {guard}\n#include <stdint.h>\n{body}#endif\n")


def defines(q):
    return "".join(f"#define {k} {v}\n" for k, v in q.items())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", default=os.path.join(ROOT, "build-accel", "mnist"))
    args = parser.parse_args()
    data = load(args.data)
    rng = np.random.default_rng(20261110)
    train_x = data["train_x"].reshape(-1, 784).astype(np.float32) / 255
    test_x = data["test_x"].reshape(-1, 784).astype(np.float32) / 255
    mlp = train_mlp(train_x, data["train_y"], rng)
    cnn = train_cnn(train_x, data["train_y"], rng)
    cal = train_x[rng.choice(len(train_x), 2000, replace=False)]
    qm, qc = quantize_mlp(mlp, cal), quantize_cnn(cnn, cal)

    xq = quantize_input(data["test_x"].reshape(-1, 784))
    labels = data["test_y"]
    float_mlp = (np.maximum(test_x @ mlp["w1"] + mlp["b1"], 0) @ mlp["w2"] + mlp["b2"]).argmax(1)
    a1 = np.maximum(im2col(test_x.reshape(-1, 28, 28, 1), 2) @ cnn["c1"] + cnn["d1"], 0)
    a2 = np.maximum(im2col(a1, 2) @ cnn["c2"] + cnn["d2"], 0)
    float_cnn = (a2.reshape(len(test_x), -1) @ cnn["f"] + cnn["e"]).argmax(1)
    int_mlp, int_cnn = mlp_int(qm, xq), cnn_int(qc, xq)
    accuracy = {
        "mlp_float": float((float_mlp == labels).mean()), "mlp_int8": float((int_mlp.argmax(1) == labels).mean()),
        "cnn_float": float((float_cnn == labels).mean()), "cnn_int8": float((int_cnn.argmax(1) == labels).mean()),
    }

    os.makedirs(FIXTURES, exist_ok=True)
    images = xq[:IMAGES]
    mlp_scalars = {"MLP_SCALE1": qm["scale1"], "MLP_SHIFT1": qm["shift1"], "MLP_SCALE2": qm["scale2"], "MLP_SHIFT2": qm["shift2"]}
    cnn_scalars = {"CNN_SCALE1": qc["s1"], "CNN_SHIFT1": qc["h1"], "CNN_SCALE2": qc["s2"], "CNN_SHIFT2": qc["h2"],
                   "CNN_SCALE3": qc["s3"], "CNN_SHIFT3": qc["h3"]}
    body = defines(mlp_scalars) + defines(cnn_scalars)
    body += c_array("mlp_w1", qm["w1"], "int8_t") + c_array("mlp_b1", qm["b1"], "int32_t")
    body += c_array("mlp_w2", qm["w2"], "int8_t") + c_array("mlp_b2", qm["b2"], "int32_t")
    # CNN weights as (K, O) with K in (ky, kx, channel) order.
    body += c_array("cnn_c1", qc["c1"], "int8_t") + c_array("cnn_d1", qc["d1"], "int32_t")
    body += c_array("cnn_c2", qc["c2"], "int8_t") + c_array("cnn_d2", qc["d2"], "int32_t")
    body += c_array("cnn_f", qc["f"], "int8_t") + c_array("cnn_e", qc["e"], "int32_t")
    open(os.path.join(GUEST, "model_cpu.h"), "w").write(header("MODEL_CPU_H", body))
    for n in SIZES:
        body = defines(mlp_scalars) + defines(cnn_scalars)
        for name, w, b in (("mlp_l1", qm["w1"], qm["b1"]), ("mlp_l2", qm["w2"], qm["b2"]), ("cnn_l1", qc["c1"], qc["d1"]),
                           ("cnn_l2", qc["c2"], qc["d2"]), ("cnn_l3", qc["f"], qc["e"])):
            body += c_array(name + "_tiles", tiles(w, n), "int8_t") + c_array(name + "_bias", padded_bias(b, n), "int32_t")
        open(os.path.join(GUEST, f"model_tiled_n{n}.h"), "w").write(header(f"MODEL_TILED_N{n}_H", body))
    body = f"#define IMAGE_COUNT {IMAGES}\n"
    # Padding after the images lets a dense gather read whole chunks past the last pixel.
    body += c_array("images", np.concatenate([images.reshape(-1), np.zeros(16, np.int8)]), "int8_t")
    body += c_array("labels", labels[:IMAGES], "uint8_t")
    body += c_array("expected_mlp", int_mlp[:IMAGES], "int8_t") + c_array("expected_cnn", int_cnn[:IMAGES], "int8_t")
    open(os.path.join(GUEST, "images.h"), "w").write(header("IMAGES_H", body))
    report = {"dataset": "MNIST (LeCun, Cortes, Burges), test set of 10,000 images", "accuracy": accuracy,
              "mlp": {k: int(v) for k, v in mlp_scalars.items()}, "cnn": {k: int(v) for k, v in cnn_scalars.items()},
              "images": IMAGES, "labels": labels[:IMAGES].tolist(),
              "expected_mlp": int_mlp[:IMAGES].astype(int).tolist(), "expected_cnn": int_cnn[:IMAGES].astype(int).tolist()}
    json.dump(report, open(os.path.join(FIXTURES, "model.json"), "w"), indent=1)
    print(json.dumps(accuracy))


if __name__ == "__main__":
    main()
