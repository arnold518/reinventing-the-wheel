"use strict";

/*
 * Component explorer: the tree is the planned fidelity.
 *
 * Expanded means "build this part from its parts" (structural), collapsed
 * means "build it as one block" (behavioral). The arrow edits that plan, and
 * the canvas follows it at once for parts that exist; Apply & Run rebuilds
 * the circuit to match. The S/B badge is read-only and shows the fidelity
 * that is built now. Parts with one form have no arrow. The plan is a sparse
 * map of exact component paths to requested fidelities.
 * Rows are virtualized so a fully structural CPU never creates tens of
 * thousands of DOM nodes.
 */
(function installComponentExplorer(global) {
  const ROW_HEIGHT = 28;
  const OVERSCAN_ROWS = 10;
  const WIDTH_STORAGE_KEY = "circuitsim.explorer.width";
  const OPEN_STORAGE_KEY = "circuitsim.explorer.open";

  function mapsEqual(left, right) {
    const leftKeys = Object.keys(left || {}).sort();
    const rightKeys = Object.keys(right || {}).sort();
    return leftKeys.length === rightKeys.length
      && leftKeys.every((key, index) => (
        key === rightKeys[index] && left[key] === right[key]
      ));
  }

  function requestedFidelity(component, overrides) {
    return overrides[component.id] || component.fidelity;
  }

  class ComponentExplorer {
    constructor(options) {
      this.panel = options.panel;
      this.viewport = options.viewport;
      this.spacer = options.spacer;
      this.rowsLayer = options.rowsLayer;
      this.search = options.search;
      this.applyButton = options.applyButton;
      this.revertButton = options.revertButton;
      this.status = options.status;
      this.toggleButton = options.toggleButton;
      this.closeButton = options.closeButton;
      this.resizer = options.resizer;
      this.onFocus = options.onFocus;
      this.onApply = options.onApply;
      this.onViewportChanged = options.onViewportChanged;
      this.onOpenChanged = options.onOpenChanged;
      this.onDraftChanged = options.onDraftChanged || (() => {});
      // Parts that exist only after Apply & Run, from a build-only preview.
      this.preview = new Map();

      this.components = new Map();
      this.rootId = null;
      this.profile = {
        editable: false,
        revision: 0,
        exactOverrides: {},
      };
      this.appliedOverrides = {};
      this.draftOverrides = {};
      this.visibleRows = [];
      this.selectedId = null;
      this.pendingFocusId = null;
      this.renderPending = false;
      this.searchTimer = null;

      this.restorePanelState();
      this.bindEvents();
    }

    restorePanelState() {
      const storedValue = localStorage.getItem(WIDTH_STORAGE_KEY);
      const storedWidth = Number(storedValue);
      const width = storedValue !== null && Number.isFinite(storedWidth)
        ? Math.max(240, Math.min(storedWidth, 560))
        : 320;
      document.documentElement.style.setProperty(
        "--explorer-width",
        `${width}px`,
      );
      // Embedded/narrow views start unobstructed, without changing the saved
      // desktop preference shared by same-origin iframes.
      const open = !document.body.classList.contains("compact-view")
        && localStorage.getItem(OPEN_STORAGE_KEY) !== "false";
      this.setOpen(open, { persist: false, notify: false });
    }

    setOpen(open, { persist = true, notify = true } = {}) {
      document.body.classList.toggle("explorer-open", open);
      this.toggleButton.setAttribute("aria-pressed", String(open));
      this.toggleButton.setAttribute("aria-expanded", String(open));
      this.panel.inert = !open;
      if (persist && !document.body.classList.contains("compact-view")) {
        localStorage.setItem(OPEN_STORAGE_KEY, String(open));
      }
      if (notify) {
        if (this.onOpenChanged) this.onOpenChanged(open);
        this.onViewportChanged();
      }
      this.scheduleRender();
    }

    bindEvents() {
      this.viewport.addEventListener("scroll", () => this.scheduleRender());
      this.search.addEventListener("input", () => {
        clearTimeout(this.searchTimer);
        this.searchTimer = setTimeout(() => {
          this.viewport.scrollTop = 0;
          this.rebuildVisibleRows();
        }, 80);
      });
      this.applyButton.addEventListener("click", () => {
        if (!this.isDirty()) return;
        this.onApply({
          revision: this.profile.revision,
          exactOverrides: { ...this.draftOverrides },
          pendingFocusId: this.pendingFocusId,
        });
      });
      this.revertButton.addEventListener("click", () => {
        this.draftOverrides = { ...this.appliedOverrides };
        this.pendingFocusId = null;
        this.rebuildVisibleRows();
        this.updateStatus();
        this.onDraftChanged();
      });
      this.toggleButton.addEventListener("click", () => {
        const next = !document.body.classList.contains("explorer-open");
        this.setOpen(next);
      });
      this.closeButton.addEventListener("click", () => this.setOpen(false));

      this.resizer.addEventListener("pointerdown", (event) => {
        event.preventDefault();
        this.resizer.setPointerCapture(event.pointerId);
        const startX = event.clientX;
        const current = parseFloat(
          getComputedStyle(document.documentElement)
            .getPropertyValue("--explorer-width"),
        ) || 320;
        const move = (moveEvent) => {
          const width = Math.max(
            240,
            Math.min(560, current + moveEvent.clientX - startX),
          );
          document.documentElement.style.setProperty(
            "--explorer-width",
            `${width}px`,
          );
          localStorage.setItem(WIDTH_STORAGE_KEY, String(width));
          this.onViewportChanged();
        };
        const finish = () => {
          this.resizer.removeEventListener("pointermove", move);
          this.resizer.removeEventListener("pointerup", finish);
          this.resizer.removeEventListener("pointercancel", finish);
        };
        this.resizer.addEventListener("pointermove", move);
        this.resizer.addEventListener("pointerup", finish);
        this.resizer.addEventListener("pointercancel", finish);
      });
    }

    setModel(components, rootId, profile) {
      this.components = components;
      this.rootId = rootId;
      this.profile = {
        editable: Boolean(profile && profile.editable),
        revision: Number(profile && profile.revision) || 0,
        exactOverrides: {
          ...((profile && profile.exactOverrides) || {}),
        },
      };
      this.appliedOverrides = { ...this.profile.exactOverrides };
      this.draftOverrides = { ...this.profile.exactOverrides };
      this.preview = new Map();  // A new build: earlier previews are stale.
      this.rebuildVisibleRows();
      this.updateStatus();
    }

    setSelected(componentId) {
      this.selectedId = componentId;
      this.scheduleRender();
      const index = this.visibleRows.findIndex(
        (row) => row.component.id === componentId,
      );
      if (index < 0) return;
      const top = index * ROW_HEIGHT;
      const bottom = top + ROW_HEIGHT;
      if (top < this.viewport.scrollTop) {
        this.viewport.scrollTop = top;
      } else if (
        bottom
        > this.viewport.scrollTop + this.viewport.clientHeight
      ) {
        this.viewport.scrollTop =
          bottom - this.viewport.clientHeight;
      }
    }

    isDirty() {
      return !mapsEqual(
        this.appliedOverrides,
        this.draftOverrides,
      );
    }

    // Build-only preview of the draft: records with id, name, type,
    // parentIndex, fidelity, availableFidelities and profileSelectable.
    setPreview(components) {
      this.preview = new Map();
      for (const record of components || []) {
        this.preview.set(record.id, { ...record, childIds: [], preview: true });
      }
      const list = components || [];
      for (const record of list) {
        if (record.parentIndex == null) continue;
        const parent = this.preview.get(list[record.parentIndex]?.id);
        if (parent) parent.childIds.push(record.id);
      }
      this.rebuildVisibleRows();
    }

    // The fidelity built now, independent of the plan.
    builtMode(component) {
      if (component.fidelity === "behavioral") return "behavioral";
      if (component.fidelity === "unspecified") {
        return component.childIds && component.childIds.length
          ? "structural"
          : "fixed";
      }
      return "structural";
    }

    createPlaceholderRow(parent, depth, index) {
      const row = document.createElement("div");
      row.className = "explorer-row placeholder";
      row.style.top = `${index * ROW_HEIGHT}px`;
      row.style.paddingLeft = `${8 + depth * 14 + 22}px`;
      row.textContent = "Its parts appear after Apply & Run";
      row.title = `${parent.id} is built as one block now`;
      return row;
    }

    modeFor(component) {
      const requested = requestedFidelity(
        component,
        this.draftOverrides,
      );
      if (requested === "behavioral") return "behavioral";
      if (component.fidelity === "unspecified") {
        return component.childIds && component.childIds.length
          ? "structural"
          : "fixed";
      }
      return "structural";
    }

    rebuildVisibleRows() {
      const rows = [];
      const query = this.search.value.trim().toLowerCase();
      const matching = query ? new Set() : null;

      if (matching) {
        for (const component of this.components.values()) {
          const text =
            `${component.name} ${component.type} ${component.id}`
              .toLowerCase();
          if (!text.includes(query)) continue;
          let current = component;
          while (current && !matching.has(current.id)) {
            matching.add(current.id);
            current = current.parentId
              ? this.components.get(current.parentId)
              : null;
          }
        }
      }

      const walkPreview = (record, depth) => {
        if (!record) return;
        rows.push({ component: record, depth, preview: true });
        if (this.modeFor(record) !== "structural") return;
        if (record.childIds.length) {
          for (const childId of record.childIds) walkPreview(this.preview.get(childId), depth + 1);
        } else if (record.fidelity === "behavioral") {
          rows.push({ placeholder: true, parent: record, depth: depth + 1 });
        }
      };
      const walk = (component, depth) => {
        if (!component || (matching && !matching.has(component.id))) {
          return;
        }
        rows.push({ component, depth });
        const children = component.childIds || [];
        const planned = this.modeFor(component) === "structural";
        // Planned structural but built as one block: its parts exist only
        // after Apply & Run. Show them from the preview (gray), or a
        // placeholder until the preview arrives.
        if (!matching && planned && !children.length && component.fidelity === "behavioral") {
          const previewed = this.preview.get(component.id);
          if (previewed && previewed.childIds.length) {
            for (const childId of previewed.childIds) walkPreview(this.preview.get(childId), depth + 1);
          } else {
            rows.push({ placeholder: true, parent: component, depth: depth + 1 });
          }
          return;
        }
        // A search also finds built parts inside a collapsed part.
        if (!matching && !planned) return;
        for (const childId of children) {
          walk(this.components.get(childId), depth + 1);
        }
      };
      walk(this.components.get(this.rootId), 0);
      this.visibleRows = rows;
      this.spacer.style.height = `${rows.length * ROW_HEIGHT}px`;
      this.viewport.dataset.rowCount = String(rows.length);
      const maximumScroll = Math.max(
        0,
        rows.length * ROW_HEIGHT - this.viewport.clientHeight,
      );
      this.viewport.scrollTop = Math.min(
        this.viewport.scrollTop,
        maximumScroll,
      );
      this.scheduleRender();
    }

    scheduleRender() {
      if (this.renderPending) return;
      this.renderPending = true;
      requestAnimationFrame(() => {
        this.renderPending = false;
        this.renderRows();
      });
    }

    renderRows() {
      const start = Math.max(
        0,
        Math.floor(this.viewport.scrollTop / ROW_HEIGHT)
          - OVERSCAN_ROWS,
      );
      const count = Math.ceil(
        this.viewport.clientHeight / ROW_HEIGHT,
      ) + OVERSCAN_ROWS * 2;
      const end = Math.min(this.visibleRows.length, start + count);
      const fragment = document.createDocumentFragment();

      for (let index = start; index < end; index += 1) {
        const row = this.visibleRows[index];
        fragment.appendChild(row.placeholder
          ? this.createPlaceholderRow(row.parent, row.depth, index)
          : this.createRow(row.component, row.depth, index, Boolean(row.preview)));
      }
      this.rowsLayer.replaceChildren(fragment);
    }

    createRow(component, depth, index, preview = false) {
      const row = document.createElement("div");
      row.className = preview ? "explorer-row preview" : "explorer-row";
      if (component.id === this.selectedId) {
        row.classList.add("selected");
      }
      if (component.id in this.draftOverrides) {
        row.classList.add("explicit-override");
      }
      row.style.top = `${index * ROW_HEIGHT}px`;
      row.style.paddingLeft = `${8 + depth * 14}px`;
      row.title =
        `${component.id}\n${component.type}\n`
        + `${this.modeFor(component)} fidelity`;
      row.addEventListener("click", () => {
        if (preview) return;  // Not built yet: nothing to show on the canvas.
        this.selectedId = component.id;
        this.onFocus(component.id);
        this.scheduleRender();
      });

      const mode = this.modeFor(component);
      const available = component.availableFidelities || [];
      const selectable = Boolean(
        this.profile.editable
        && component.profileSelectable
        && available.includes("structural")
        && available.includes("behavioral"),
      );
      const built = this.builtMode(component);
      if (!preview && mode !== built) row.classList.add("pending");
      const arrow = document.createElement("button");
      arrow.type = "button";
      arrow.className = "explorer-arrow";
      arrow.textContent = selectable
        ? mode === "structural" ? "▾" : "▸"
        : "•";
      arrow.disabled = !selectable;
      arrow.setAttribute("aria-expanded", String(selectable && mode === "structural"));
      arrow.title = selectable
        ? mode === "structural"
          ? "Collapse: build as one behavioral block (Apply & Run)"
          : "Expand: build from its parts (Apply & Run)"
        : "This component has one fixed implementation";
      arrow.addEventListener("click", (event) => {
        event.stopPropagation();
        if (!selectable) return;
        const next = mode === "structural"
          ? "behavioral"
          : "structural";
        this.draftOverrides[component.id] = next;
        if (
          next === "structural"
          && (!component.childIds || !component.childIds.length)
        ) {
          this.pendingFocusId = component.id;
        }
        this.rebuildVisibleRows();
        this.updateStatus();
        this.onDraftChanged();
      });

      const name = document.createElement("span");
      name.className = "explorer-name";
      name.textContent = component.name;

      // Read-only: the fidelity built and simulated now. A previewed part is
      // not built, so it has no badge.
      const badge = document.createElement("span");
      if (preview) {
        badge.className = "explorer-badge preview";
        row.title = `${component.id}\n${component.type}\nbuilt after Apply & Run`;
        row.append(arrow, name, badge);
        if (component.id in this.draftOverrides) row.appendChild(this.createResetButton(component));
        return row;
      }
      badge.className = `explorer-badge ${built}`;
      badge.textContent =
        built === "structural" ? "S" : built === "behavioral" ? "B" : "fixed";
      badge.title = built === "fixed"
        ? "One fixed implementation"
        : `Built ${built}${mode !== built ? `; Apply & Run makes it ${mode}` : ""}`;

      row.append(arrow, name, badge);

      if (component.id in this.draftOverrides) row.appendChild(this.createResetButton(component));
      return row;
    }

    createResetButton(component) {
      const reset = document.createElement("button");
      reset.type = "button";
      reset.className = "explorer-reset";
      reset.textContent = "↶";
      reset.title = "Return this component to its inherited profile";
      reset.addEventListener("click", (event) => {
        event.stopPropagation();
        delete this.draftOverrides[component.id];
        this.rebuildVisibleRows();
        this.updateStatus();
        this.onDraftChanged();
      });
      return reset;
    }

    updateStatus(message = null, error = false) {
      const changed = new Set([
        ...Object.keys(this.appliedOverrides),
        ...Object.keys(this.draftOverrides),
      ]);
      let count = 0;
      for (const path of changed) {
        if (
          this.appliedOverrides[path]
          !== this.draftOverrides[path]
        ) count += 1;
      }
      this.applyButton.disabled =
        !this.profile.editable || count === 0;
      this.revertButton.disabled = count === 0;
      this.status.classList.toggle("error", error);
      if (message) {
        this.status.textContent = message;
      } else if (!this.profile.editable) {
        this.status.textContent = "This scenario has a fixed circuit";
      } else if (count) {
        this.status.textContent =
          `${count} unapplied ${count === 1 ? "change" : "changes"}`;
      } else {
        this.status.textContent =
          `Applied profile ${this.profile.revision}`;
      }
    }

    setApplying() {
      this.applyButton.disabled = true;
      this.revertButton.disabled = true;
      this.status.classList.remove("error");
      this.status.textContent = "Building and running…";
    }
  }

  global.CircuitExplorer = ComponentExplorer;
}(window));
