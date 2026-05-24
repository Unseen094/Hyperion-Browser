# Next Steps: From Here to 100%

---

## Immediate Actions (This Week)

### 1. **Create GitHub Projects Board** ⏳

```bash
# Create a new project board via GitHub CLI (requires gh auth)
gh project create --name "Hyperion 1.0 Roadmap to 100%" --org anomalyco --visibility all

# Create columns: Backlog | In Progress | Review | Done
# Import phases from final.md as issues? No — too many items.
# Instead, create 12 **Epics** (Month 1–12) and begin splitting into tasks per epic.
```

**Alternative**: Use a **single issue** labeled `Phase 0: Project Setup` with task list sub-issues (GitHub checkboxes) referencing `final.md`.

### 2. **Assign Owners** 👥

Create a CONTRIBUTING.md or TEAM.md with:
```markdown
| Lane | Lead | Repo Access |
|------|------|-----------|
| JavaScript Engine | Jane | r/+ |
| WASM | John | r/+ |
| Security | Alex | r/+ |
| Media | Sam | r/+ |
| Layout/CSS | Leo | r/+ |
| Storage/Network | Mia | r/+ |
```

### 3. **Infrastructure Kickoff** 🛠️

| Task | Command | Owner |
|------|---------|-------|
| Setup project board | `gh project create` | Build |
| Create epics issue | `gh issue create --title "Hyperion 1.0 Epics"` | PM |
| Add CODEOWNERS file | `.github/CODEOWNERS` | Security |
| Setup sccache/Ccache | `vcpkg install sccache` + CMake toolchain | Build |
```cmake
# In CMakeLists.txt
set(CMAKE_C_COMPILER_LAUNCHER sccache)
set(CMAKE_CXX_COMPILER_LAUNCHER sccache)
```

---


## Phase 0 Breakdown (Week 0, Project Setup)

Split `final.md` Phase 0 into 3 sub-phases:

```markdown
Phase 0.1: Project Board + Epics Issue Created ✅ DONE (see final.md)
Phase 0.2: Repo Renaming / Forking for Security (Optional)
  - Rename `hyperion_browser` to `Hyperion`
  - Update CMake, CI, docs, Python venv
Phase 0.3: CI/CD Overhaul
  - Add Windows + Ubuntu + macOS runners
  - Upload artifacts to S3 or GitHub Releases
  - Enable `sccache` for C++/C/Rust
```

**Tooling to install**:
- `sccache`, `ccache`, `ninja`
- `nodejs` for Web Platform Tests runner
- `python3.11` venv with `pip install -r tools/requirements.txt`

---


## Prioritized Next Steps (In Order)


### 🔥 **Critical Path**

| Step | What | How | Owner | Est. Time |
|------|------|-----|-------|-----------|
| 1 | Create GitHub Projects board | `gh project create` then add Epics to final.md | PM | 1h |
| 2 | Add CODEOWNERS to repo | Edit `.github/CODEOWNERS` for all lanes | Security | 1h |
| 3 | Setup sccache CMake | Fork CMake toolchain file, add to repo | Build | 2h |
| 4 | Create epics issue | GitHub issue with task list referencing final.md months | PM | 1h |
| 5 | Add CI matrix for Windows/Ubuntu/macOS | `{% matrix: [windows-latest, ubuntu-latest, macos-latest] %}` | Build | 3h |
| 6 | Upload engine artifacts to GitHub Releases | CMake `install(...)` then `gh release create` | Release | 2h |


---


## Week 0–1 Checklist

- [ ] Create GitHub Project Board with 12 Epics (Month1–Month12)
- [ ] `.github/CODEOWNERS` with lane owners
- [ ] `sccache` enabled in CMake toolchain (C/C++/Rust)
- [ ] CI matrix (Windows, Linux, macOS) added and tested
- [ ] Weekly meetings scheduled (Mon/Wed/Fri 15min)
- [ ] Git convention: `feat(js): Add BigInt optimizations` or `fix(media): Enable VP9 profile 2`
- [ ] Issue template `PHASE_TEMPLATE.md` for new phases
- [ ] `final.md` referenced in root README and team agreements

---


## Decision Points to Resolve

Question: Should we create 60 individual issues (one per phase) or use Epics?
Answer: Start with **12 epics** (one per month), then allow each lane lead to split phases into sub-issues during the month's sprint. This keeps repo noise low.

Example:
```
issues/
├─ EPIC_01_JS_ENGINE.md # refers to P01–P14, labeled epic
├─ EPIC_02_SECURITY.md # refers to S01–S08
├─ ...
```

---


## Documentation to Update

1. **README.md** → Add banner: *Hyperion Browser 1.0 Roadmap to 100% embedded* (link to final.md)
2. **CONTRIBUTING.md** → Add lane owners and contribution guides per lane
3. **docs/ARCHITECTURE.md** → Update diagrams with new JS JIT path (baseline → optimizing → machine code)
4. **.github/workflows/ci.yml** → Add `sccache`, Ninja build
5. **tools/requirements.txt** → Add `sccache`, `cmake>=3.28`, `ninja`

---

## Success Metrics for Week 0

- ✅ Project board created and 12 epics visible
- ✅ CODEOWNERS merged
- ✅ CI runs green on all 3 platforms
- ✅ sccache caching >3x compile speedup verified
- ✅ Issue templates merged (Phase task template, Bug report, RFC)

---

**Next Action for you**:

Shall I:
1. ✅ Create GitHub Codespaces devcontainer for unified environment?
2. ⏳ Update README.md with 100% roadmap banner and link to final.md
3. ⏳ Create CI/CD yaml for new matrix runners
4. 🔧 Setup CODEOWNERS and Teams in the repo

Reply with your preference, or I’ll proceed with the **minimal setup**: add 12 epics issue task list referencing final.md and enable sccache in CMake.
