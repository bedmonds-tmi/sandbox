To open this repo in browser VS Code

From a fresh clone, run this from the repo root:

```sh
docker compose up --build
```

After the first run creates the internal `dev` folder, keep running Compose from the parent workspace directory:

```sh
docker compose -f dev/compose.yaml up --build
```

Do not `cd dev` before running the command above. The `-f dev/compose.yaml` path keeps `/workspace` pointed at the parent Zephyr workspace.

Then open:

```text
http://localhost:8080
```

Compose will build the `zephyr-dev` image the first time it runs.

To use the same container from a shell instead of browser VS Code, run:

```sh
docker compose -f dev/compose.yaml run --rm zephyr-dev bash
```
