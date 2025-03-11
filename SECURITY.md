# Security Policy

## Supported Versions

 * This repository does not currently maintain release branches.  **Only the latest release is supported.**

 * If a security issue is identified in a current release, the fix will trigger a new release from `main`.

 * If a security issue is identified in any release, we will disclose the issue and advise everyone to upgrade to the latest release.


## Reporting a Vulnerability

Per Google policy, please use https://g.co/vulnz to report security vulnerabilities.  Google uses this for intake and triage.  For valid issues, we will do coordination and disclosure here on GitHub (including using a GitHub Security Advisory when necessary).

The Google Security Team will process your report within a day, and respond within a week (although it will depend on the severity of your report).


## Remediation Actions

 * A GitHub issue will be created with the `type: vulnerability` label to coordinate a response.  After remediation, we will also use this issue to disclose any details we withheld between receiving the private report and resolving the issue.

 * A GitHub Security Advisory may be created, if appropriate.  For example, this would be done if the issue impacts users or dependent projects.  This might be skipped for other issues, such as CI workflow vulnerabilities.

 * Vulnerabilities in NPM modules will be reported to NPM so that they show up in `npm audit`.


