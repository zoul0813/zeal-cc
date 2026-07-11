import json
import os
root = os.path.join("./", ".deploy", "hostfs")
paths = []
for dirpath, _, files in os.walk(root):
    for filename in files:
        if filename == "index.json":
            continue
        full_path = os.path.join(dirpath, filename)
        rel_path = os.path.relpath(full_path, root).replace(os.sep, "/")
        paths.append(rel_path)
paths.sort()
with open(os.path.join(root, "index.json"), "w") as handle:
    json.dump(paths, handle)