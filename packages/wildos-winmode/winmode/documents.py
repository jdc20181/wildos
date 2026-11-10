"""Which Windows apps hold work not yet saved, for the question asked before
Windows shuts down or restarts (board 9.9 and the panel's Restart and Shut
down). No I/O here, so it can be tested on its own.

What Windows can tell, from best to least:
  - Word, Excel and PowerPoint: Office's own Saved property for each
    document, read by wild-guest over COM ("documents" request). Only the
    instance Windows hands out for each app is reached; a document not in
    that list goes by its window title like any other app.
  - Apps that mark unsaved work in their title, with a "*" (Notepad,
    Notepad++, Paint.NET) or a dot (Visual Studio Code, Sublime Text): a
    marker means unsaved, its absence means saved.
  - Anything else: open, state unknown. Most apps say nothing in their
    title, so an unknown app is never reported as saved.
"""

import os
import re

# Programs that put a marker in their window title while there are unsaved
# changes, so a title without one means everything is saved.
MARKS_UNSAVED = {"notepad", "notepad++", "code", "sublime_text", "paintdotnet"}
# Programs that never hold a document of their own.
NOTHING_TO_SAVE = {"explorer", "calc", "calculatorapp", "systemsettings", "taskmgr", "charmap", "mmc", "control",
                   "applicationframehost", "msiexec", "setup"}
OFFICE = {"winword": "Word", "excel": "Excel", "powerpnt": "PowerPoint"}
MARKERS = ("*", "\u25cf", "\u2022")


def stem(image):
    """C:\\Program Files\\Microsoft Office\\root\\Office16\\WINWORD.EXE -> winword"""
    base = re.split(r"[\\/]", image or "")[-1]
    return os.path.splitext(base)[0].lower()


def split_title(title):
    """A window title as (document, marked): the document's name without
    the app's name after it, and whether it carries an unsaved marker."""
    t = (title or "").strip()
    marked = False
    for mark in MARKERS:
        if t.startswith(mark):
            t, marked = t[len(mark):].strip(), True
        if t.endswith(mark):
            t, marked = t[:-len(mark)].strip(), True
    # "Budget 2027.xlsx - Excel", "*notes.txt - Notepad": the app comes last.
    if " - " in t:
        t = t.rsplit(" - ", 1)[0].strip()
    # Visual Studio Code: "file.py - folder - Visual Studio Code".
    for mark in MARKERS:
        if t.startswith(mark):
            t, marked = t[len(mark):].strip(), True
    return t, marked


def _office_match(name, docs):
    """The Office document whose name is this window's: titles drop the
    extension when Windows hides them, so the stem counts too."""
    key = (name or "").lower()
    for d in docs:
        n = str(d.get("name") or "")
        if n.lower() == key or os.path.splitext(n)[0].lower() == key:
            return d
    return None


def documents(windows, office=None, app_name=None):
    """[{app, document, unsaved: True|False|None, hwnd}] for the app windows
    open in Windows. windows: wild-guest's window list. office: wild-guest's
    Office documents, or None when Office could not be asked. app_name(image)
    gives the name Wild OS shows for a program (its Start entry), or None."""
    out = []
    office = office if isinstance(office, list) else None
    for w in windows or []:
        if not isinstance(w, dict) or not w.get("visible", True) or w.get("tool") or w.get("owner"):
            continue
        s = stem(w.get("image")) or str(w.get("app") or "")
        if s in NOTHING_TO_SAVE:
            continue
        name = (app_name(w.get("image")) if app_name else None) or OFFICE.get(s) or (s[:1].upper() + s[1:])
        document, marked = split_title(w.get("title"))
        unsaved = None
        if s in OFFICE and office is not None:
            d = _office_match(document, [d for d in office if isinstance(d, dict) and d.get("app") == s])
            if d is not None:
                document = str(d.get("name") or document)
                unsaved = not bool(d.get("saved"))
        elif marked:
            unsaved = True
        elif s in MARKS_UNSAVED:
            unsaved = False
        out.append({"app": name[:80], "document": (document or name)[:120], "unsaved": unsaved, "hwnd": w.get("hwnd")})
    # Unsaved first, then the unknown, then what is saved: what matters most
    # reads first in the list.
    rank = {True: 0, None: 1, False: 2}
    out.sort(key=lambda d: rank[d["unsaved"]])
    return out


def needs_asking(docs):
    """Before Windows itself restarts or shuts down (the panel): asked when
    something is unsaved, or might be. Only what is known to be saved, or
    nothing at all, goes ahead without a question."""
    return any(d.get("unsaved") is not False for d in docs)
