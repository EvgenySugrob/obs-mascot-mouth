# Publishing with GitHub Desktop

## Create the repository

1. Open GitHub Desktop.
2. Select **File → New repository**.
3. Use `obs-mascot-mouth` as the repository name.
4. Select the folder that contains this project.
5. Do not add another license or `.gitignore`.
6. Create the repository.

## First commit

Review the changed files in GitHub Desktop. Build directories, compiled
binaries, editor settings, and ZIP files are excluded by `.gitignore`.

Use a concise first commit:

```text
Initial release
```

Select **Commit to main**, then **Publish repository**. Clear **Keep this code
private** if the repository should be public.

## Suggested repository description

```text
Lightweight microphone-reactive 2D mascot source for OBS Studio.
```

## Suggested topics

```text
obs-studio
obs-plugin
cpp
vtuber
pngtuber
audio-reactive
```

## Before publishing

Verify that the repository does not contain:

- `build` or `build_x64`;
- `.vs` or `.vscode`;
- DLL, OBJ, or ZIP files;
- personal image assets without redistribution rights;
- tokens, passwords, or local absolute paths.
