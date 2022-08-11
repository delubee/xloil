"""
    Sets up xloil to use the PySide2 GUI toolkit. This *must* be imported before `PySide2`
    is imported to allow xlOil to own the QApplication object. *All* interaction with 
    the Qt GUI must be done on the GUI thread: use `Qt_thread().submit(...)`
"""

from . import _qtconfig

_qtconfig._QT_MODULE_NAME = "PySide2"

from ._qtgui import *

from xloil._core import XLOIL_READTHEDOCS, _fix_module_for_docs
if XLOIL_READTHEDOCS:
    from . import _qtgui
    _fix_module_for_docs(locals(), _qtgui.__name__, __name__)