import os
from pathlib import Path
import nbformat
from nbconvert.preprocessors import ExecutePreprocessor

# 1. Reliably find the input notebook relative to this script
SCRIPT_DIR = Path(__file__).resolve().parent
# If analysis.ipynb is at the root (/workspace/analysis.ipynb) and this script is in /workspace/scripts/
notebook_path = SCRIPT_DIR.parent / "analysis.ipynb"

# Fallback if analysis.ipynb is in the current working directory
if not notebook_path.exists():
    notebook_path = Path("analysis.ipynb")

if not notebook_path.exists():
    raise FileNotFoundError(f"Could not locate analysis.ipynb at {notebook_path.resolve()}")

# 2. Target the mounted volume directory from Docker (-e OUTPUT_DIR=/results)
output_dir = Path(os.getenv("OUTPUT_DIR", "/results"))
output_dir.mkdir(parents=True, exist_ok=True)
output_path = output_dir / "analysis.ipynb"

# 3. Read the template notebook
with open(notebook_path, "r", encoding="utf-8") as f:
    nb = nbformat.read(f, as_version=4)

# 4. Configure preprocessor
# timeout=-1 disables the 30-second cell execution timeout
ep = ExecutePreprocessor(timeout=-1, kernel_name="python3")

# 5. Execute the notebook
# Setting metadata path to output_dir ensures notebook file writes (e.g., plt.savefig) land in /results
ep.preprocess(nb, {"metadata": {"path": str(output_dir)}})

# 6. Save the executed notebook to the mounted host directory
with open(output_path, "w", encoding="utf-8") as f:
    nbformat.write(nb, f)

print(f"Successfully executed and saved notebook to: {output_path}")