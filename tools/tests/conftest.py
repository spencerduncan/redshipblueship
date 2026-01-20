"""
Pytest configuration for symbol tools tests.

Automatically loaded by pytest to set up the Python path.
"""

import sys
from pathlib import Path

# Add tools directory to path so tests can import detect_collisions and namespace_symbols
sys.path.insert(0, str(Path(__file__).parent.parent))
# Add tools/lib directory to path so tests can import normalizer
sys.path.insert(0, str(Path(__file__).parent.parent / "lib"))
