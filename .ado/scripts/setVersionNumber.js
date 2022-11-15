/*
 * This script sets the version number for the rest of the build.
 * After this script has run, other tasks can use the variable
 * to retrieve the build number for their logic...
 * 
 * See: https://docs.microsoft.com/en-us/azure/devops/pipelines/build/variables?view=azure-devops&tabs=yaml
 * for environment variables used in this script to compute the version number.
 */ 
const env = process.env;

function main() {

    if (env["Build_Reason"] === "PullRequest") {
        fatalError("Build script is intended for CI pipeline and should not be used for pull requests.");
    }

    const {semanticVersion, fileVersion} = computeVersion();
    console.log(`Semantic Version: ${semanticVersion}`);
    console.log(`Windows File Version: ${fileVersion}`);

    if (!fileVersion.startsWith(semanticVersion)) {
      // Update the pipeline build number to correlate it with the semantic version.
      console.log(`##vso[build.updatebuildnumber]${fileVersion} -- ${semanticVersion}`);
    }

    // Set the variables (as output) so that other jobs can use them.
    console.log(`##vso[task.setvariable variable=semanticVersion;isOutput=true]${semanticVersion}`);
    console.log(`##vso[task.setvariable variable=fileVersion;isOutput=true]${fileVersion}`);
}

function computeVersion() {
    // Compute base version;
    const sourceBranch = env["Build_SourceBranch"];
    if (sourceBranch === "refs/heads/main") {
      return computeMainVersion();
    }
    if (sourceBranch === "refs/heads/rnw/canary") {
      return computeMainVersion();
    }
    if (sourceBranch.startsWith("refs/heads/rnw/0.")) {
      return computeReleaseVersion();
    }

    fatalError(`Build script does not support source branch '${sourceBranch}'.`)
}

function computeMainVersion() {
    const buildNumber = env["Build_BuildNumber"];
    const buildNumberParts = buildNumber.split(".");
    if (buildNumberParts.length !== 4
        || buildNumberParts[0] !== '0'
        || buildNumberParts[1] !== '0'
        || buildNumberParts[2].length !== 4
        || buildNumberParts[3].length < 4
        || buildNumberParts[3].length > 5) {
        fatalError(`Unexpected pre-release build number format encountered: ${buildNumber}`)
    }

    const shortGitHash = env["Build_SourceVersion"].substring(0, 8);

    return {
        semanticVersion: `0.0.0-${buildNumberParts[2]}.${buildNumberParts[3]}-${shortGitHash}`,
        fileVersion: buildNumber
    }
}

function computeReleaseVersion() {
    const buildNumber = env["Build_BuildNumber"];
    const buildNumberParts = buildNumber.split(".");
    if (buildNumberParts.length !== 3) {
        fatalError(`Unexpected release build number format encountered: ${buildNumber}`)
    }

    return {
        semanticVersion: buildNumber,
        fileVersion: buildNumber + '.0'
    }
}

function fatalError(message) {
    console.log(`##[error]${message}`);
    process.exit(1);
}

main();