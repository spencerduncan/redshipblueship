"""
Pytest configuration for tools tests.

Adds the tools/lib directory to the Python path so that imports work correctly.
"""

import sys
from pathlib import Path

# Add tools/lib to Python path for imports
tools_lib = Path(__file__).parent.parent / "lib"
sys.path.insert(0, str(tools_lib))
