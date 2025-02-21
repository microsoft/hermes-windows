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
  const mustPublish = env["MustPublish"] === "True";
  console.log(`MustPublish: ${mustPublish}`);

  const { semanticVersion, fileVersion, isTest } = computeVersion();
  console.log(`Semantic Version: ${semanticVersion}`);
  console.log(`Windows File Version: ${fileVersion}`);

  const testPrefix = isTest ? "Test " : "";
  const publishPrefix = mustPublish ? "CI " : "PR ";
  if (!fileVersion.startsWith(semanticVersion)) {
    // Update the pipeline build number to correlate it with the semantic version.
    console.log(
      "##vso[build.updateBuildNumber]" +
        `${testPrefix}${publishPrefix}${fileVersion} -- ${semanticVersion}`
    );
  }

  // Set the variables (as output) so that other jobs can use them.
  console.log(
    `##vso[task.setVariable variable=semanticVersion;isOutput=true]${semanticVersion}`
  );
  console.log(
    `##vso[task.setVariable variable=fileVersion;isOutput=true]${fileVersion}`
  );
}

function computeVersion(mustPublish) {
  const sourceBranch = env["Build_SourceBranch"];
  const isTest = sourceBranch.includes("1es-pt-migration");

  if (isTest) {
    return computeCanaryVersion(/*isTest:*/ true);
  }
  if (!mustPublish) {
    return computeCanaryVersion(/*isTest:*/ false);
  }
  if (sourceBranch === "refs/heads/main") {
    return computeCanaryVersion(/*isTest:*/ false);
  }
  if (sourceBranch.startsWith("refs/heads/rnw/0.")) {
    return computeReleaseVersion();
  }

  fatalError(`Build script does not support source branch '${sourceBranch}'.`);
}

function computeCanaryVersion(isTest) {
  const buildNumber = env["Build_BuildNumber"];
  const buildNumberParts = buildNumber.split(".");
  if (
    buildNumberParts.length !== 4 ||
    buildNumberParts[0] !== "0" ||
    buildNumberParts[1] !== "0" ||
    buildNumberParts[2].length !== 4 ||
    buildNumberParts[3].length < 4 ||
    buildNumberParts[3].length > 5
  ) {
    fatalError(
      `Unexpected pre-release build number format encountered: ${buildNumber}`
    );
  }

  const shortGitHash = env["Build_SourceVersion"].substring(0, 8);

  return {
    semanticVersion:
      "0.0.0-" +
      `${buildNumberParts[2]}.${buildNumberParts[3]}-${shortGitHash}`,
    fileVersion: buildNumber,
    isTest,
  };
}

function computeReleaseVersion() {
  const buildNumber = env["Build_BuildNumber"];
  const buildNumberParts = buildNumber.split(".");
  if (buildNumberParts.length !== 3) {
    fatalError(
      `Unexpected release build number format encountered: ${buildNumber}`
    );
  }

  return {
    semanticVersion: buildNumber,
    fileVersion: buildNumber + ".0",
    isTest: false,
  };
}

function fatalError(message) {
  console.log(`##[error]${message}`);
  process.exit(1);
}

main();
