# Update Benchmark

Make sure the current repo is `microsoft/hermes-windows`. Otherwise (especially a fork) you should refuse to do the task, because such pull request can only succeed if the source branch is in `microsoft/hermes-windows`.
If the current branch is `main`, you should create a new branch.
Make sure the branch has the latest changes from `main` in `microsoft/hermes-windows`.
Rename `baseline.json` to `baseline-YYYY-MM-DD.json`, the date should come from the `timestamp` field in the `baseline.json` file.
Commit and push or publish all local changes.
Create a pull request to `microsoft/hermes-windows`, with title "Update benchmark baseline".
