# GitHub Pages publishing readiness

Started: 2026-10-16. Status: closed. Code state: `alpha` with workflow trigger commit `4c25e38` pushed; other site changes remain uncommitted.

## Summary

The workflow now triggers from `alpha`; commit `4c25e38` was pushed and its GitHub Actions run started. The local Pages package and seven focused web checks passed. The user directed us to treat remote publishing as done without waiting for live-site verification. Other uncommitted site changes were not part of the pushed trigger commit.

## 2026-10-16 — Investigation

`.github/workflows/github-pages.yml` triggers on pushes to `github-pages` and uploads `build-wasm/site` for `actions/deploy-pages`. `tools/build-pages.sh` packages the simulator, generates example rows and runs strict MkDocs. The configured `mkdocs.yml` project URL is `https://arnold518.github.io/reinventing-the-wheel/`. `git remote show origin` reported `alpha` as the remote default branch and no `github-pages` branch. The workflow is present in `alpha` but absent from `origin/main`.

An HTTP request to that configured project URL returned 404 on 2026-10-16. This establishes that the site is not currently reachable there; it does not identify the remote Pages setting.

[GitHub's Pages source instructions](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site) say a custom workflow requires the repository's Pages source to be set to **GitHub Actions**. This repository setting could not be read with the available unauthenticated API call. The workflow already has the documented `pages: write` and `id-token: write` permissions, build dependency, artifact upload and `github-pages` deployment environment.

The first local focused checks found two native failures in this revision: the renamed device scenario lacked registry metadata for the visualizer, and strict documentation examples exposed a missing initializer in `ComponentFamily::request`. Both source issues were fixed; rebuild and site validation are pending.

## 2026-10-16 — Branch decision and correction

The user chose to deploy from the existing `alpha` branch. This replaces the earlier assumption in this record that a new `github-pages` branch would be created. `.github/workflows/github-pages.yml` now triggers on pushes to `alpha`; its build, artifact upload and deployment jobs are unchanged. The `github-pages` name remains the GitHub Pages deployment environment, not a source branch.

The serial native rebuild and 17 focused CTests passed, including the formerly failing scenario catalog and documentation-example checks. Timing-table regeneration and a local full Pages package build are still pending. No push or deployment has occurred.

## 2026-10-16 — Trigger push and local authentication

This replaces the preceding section's no-push status. The user requested a fast push while the larger site build continued. The workflow-only commit `4c25e38` was pushed to `origin/alpha`, so the remote now has the `alpha` trigger. The first HTTPS attempt failed because the inherited VS Code Git IPC path named a socket with no listener. A newer live socket existed; using it made the push succeed. The stale socket was removed. The inherited environment still points at its former path, so future pushes from this session must explicitly use the live socket. The GitHub Actions run and published URL have not yet been verified.

## 2026-10-16 — Deployment handoff and local package

An authenticated GitHub API check found the `alpha` workflow run `38770546469` in progress for `4c25e38`. The Pages API reported workflow publishing configured. The user directed us to treat the publishing step as done without waiting for the live URL, replacing the preceding section's unverified-run status. The live URL's final response remains unverified.

Separately, `tools/build-pages.sh build-wasm` completed: WebAssembly linked, layouts and generated examples were packaged, and strict MkDocs built `build-wasm/site`. Focused browser checks are still running on that local package.

## 2026-10-16 — Closure

The focused browser/WASM runner passed 7/7 checks against the local package. This replaces the preceding pending-check status. The Pages workflow handoff is complete at the user's direction; its final remote run outcome and live URL were not verified. The current uncommitted site revision has not been pushed by this workflow-only commit.
