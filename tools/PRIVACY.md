# Local privacy checks

Before committing or pushing from any new clone, run with Python 3:

```text
python tools/privacy_guard.py install-hooks
```

This configures the repository's tracked pre-commit, commit-msg and pre-push
hooks and remembers the Python executable locally. Existing custom hook paths
require review rather than being silently overwritten. Run installation again
after moving a clone or changing Python installations.

The commit hooks inspect the complete staged tree and the proposed message.
The push hook checks all reachable file versions and commit/tag metadata for
the exact outgoing tips, including binary blobs. It also blocks publishing
local checkpoint refs. These checks reject Quest device serial patterns and
private Tailscale hostnames without printing matched content or filenames.

For other personal identifiers, put one private value per line in the local
Git metadata file `info/privacy-denylist` (normally `.git/info/privacy-denylist`).
Never commit that file or copy its contents into a shared command or report.
Values are checked case-insensitively. Store only intentional private values;
short or common words can block legitimate changes. Keep old repository
backups and replacement lists local, outside the published history.

Manual checks:

```text
python tools/privacy_guard.py staged
python tools/privacy_guard.py message PATH_TO_LOCAL_MESSAGE_FILE
python tools/privacy_guard.py history
python tools/tests/test-privacy-guard.py
```

The history command checks all local refs, which can include private checkpoint
history absent from the branch being pushed. A failed check prints no matched
values. Diagnose locally; share only sanitized counts and locations.

Hooks need installation in each clone, including cloud development clones.
They can be bypassed and do not replace review or a credential scanner such as
Gitleaks. They do not sanitize ADB output: capture device selection locally and
report only "personal Quest 3 device", as required by AGENTS.md.
