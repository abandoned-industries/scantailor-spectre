# Claude Code Permissions for ScanTailor Spectre

## Bash Commands - Allow

### Build
- cmake
- cmake --build
- make
- make -j*
- xcodebuild

### Git (read-only and local modifications)
- git status
- git diff
- git log
- git show
- git fetch
- git branch
- git add
- git commit
- git stash
- git checkout
- git merge
- git rebase

### macOS Signing & Packaging
- codesign
- xcrun notarytool
- xcrun stapler
- hdiutil create
- hdiutil attach
- hdiutil detach
- ditto
- macdeployqt

### Documentation
- pandoc

### Package Management
- brew install
- brew --prefix
- brew list
- brew info

### Read-only / Safe Operations
- open
- ls
- find
- grep
- wc
- cat
- tail
- head
- file
- otool
- nm
- strings
- diff
- stat

### File Creation/Modification (no deletion)
- mkdir
- cp
- mv
- touch

### Process Inspection
- pgrep
- ps

## Bash Commands - REQUIRE APPROVAL

### Git Remote Operations (modifies GitHub)
- git push
- git push --force
- gh pr create
- gh pr merge
- gh release create
- gh release delete
- gh repo delete

### Git Destructive Operations (loses uncommitted work)
- git reset --hard
- git clean

### Destructive File Operations
- rm
- rm -rf
- rmdir
- trash

### Process Termination
- kill
- killall
- pkill

### System Modifications
- sudo
- chown
- chmod (except on build artifacts)

### Network Operations
- curl (POST/PUT/DELETE)
- wget

## File Operations - Allow

- Read any file in /Users/kazys/Developer/scantailor-weasel
- Write/Edit any file in /Users/kazys/Developer/scantailor-weasel
- Create new files in /Users/kazys/Developer/scantailor-weasel

## File Operations - REQUIRE APPROVAL

- Delete any file
- Modify files outside /Users/kazys/Developer/scantailor-weasel
