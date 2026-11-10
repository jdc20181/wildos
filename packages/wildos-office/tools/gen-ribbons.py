#!/usr/bin/env python3
"""Writes the Wild OS ribbons for Write and Tables.

    tools/gen-ribbons.py            regenerate lo/ui/{swriter,scalc}/notebookbar_wildos.ui
    tools/gen-ribbons.py --labels   print the command label overrides the ribbons expect

The .ui files are GtkBuilder files in LibreOffice's notebookbar dialect. They
are long and repetitive, so the ribbon is described here as data and the
files are generated; edit this script, not the .ui files.

Button text on a ribbon comes from each command's label in LibreOffice's
registry, never from the .ui file, so the short Wild OS names ("Picture",
"Replace", "Font colour") need registry overrides. --labels prints them as
component-data blocks for wildos.xcd.
"""

import os
import sys
from xml.sax.saxutils import escape

HERE = os.path.dirname(os.path.abspath(__file__))
UI = os.path.join(HERE, "..", "lo", "ui")

FIND = "vnd.sun.star.findbar:FocusToFindbar"


# Ribbon description. A tab is (id, label, groups); a group is
# (id, label, priority, columns). A column is one of
#   ("big", [items])          large icons with the label underneath
#   ("rows", [row, row])      up to three stacked rows, each a list of items
# and an item is "command" followed by optional flags: "|menu" for a
# drop-down arrow, "|text" to show the label beside a small icon, "|w=N" to
# fix the width of a box such as the font name.
# Bigger priority numbers fold away first in a narrow window, into the >>
# button at the end of the ribbon.

def big(*items):
    return ("big", list(items))


def rows(*rs):
    return ("rows", [list(r) for r in rs])


FILE_MENU = {
    "swriter": [
        ".uno:AddDirect", ".uno:Open", ".uno:RecentFileList", "-",
        ".uno:Save", ".uno:SaveAs", ".uno:SaveACopy", "-",
        ".uno:ExportToPDF", ".uno:ExportTo", ".uno:SendMailDocAsPDF", "-",
        ".uno:PrintPreview", ".uno:Print", "-",
        ".uno:SetDocumentProperties", ".uno:Signature", "-",
        ".uno:OptionsTreeDialog", ".uno:HelpIndex", "-",
        ".uno:CloseDoc",
    ],
    "scalc": [
        ".uno:AddDirect", ".uno:Open", ".uno:RecentFileList", "-",
        ".uno:Save", ".uno:SaveAs", ".uno:SaveACopy", "-",
        ".uno:ExportToPDF", ".uno:ExportTo", ".uno:SendMailDocAsPDF", "-",
        ".uno:PrintPreview", ".uno:Print", ".uno:PrinterSetup", "-",
        ".uno:SetDocumentProperties", ".uno:Signature", "-",
        ".uno:OptionsTreeDialog", ".uno:HelpIndex", "-",
        ".uno:CloseDoc",
    ],
}

CLIPBOARD = ("Clipboard", "Clipboard", 1, [
    big(".uno:Paste|menu"),
    rows([".uno:Cut|text"], [".uno:Copy|text"]),
])

WRITER = [
    ("Home", "Home", [
        CLIPBOARD,
        ("Text", "Text", 1, [
            rows([".uno:CharFontName|w=140", ".uno:FontHeight|w=70"],
                 [".uno:Bold", ".uno:Italic", ".uno:Underline|menu", ".uno:Strikeout",
                  ".uno:Color|menu", ".uno:CharBackColor|menu"]),
        ]),
        ("Paragraph", "Paragraph", 2, [
            rows([".uno:DefaultBullet", ".uno:DefaultNumbering", ".uno:IncrementIndent",
                  ".uno:LineSpacing|menu"],
                 [".uno:LeftPara", ".uno:CenterPara", ".uno:RightPara", ".uno:JustifyPara"]),
        ]),
        ("Styles", "Styles", 4, [
            big(".uno:TextBodyParaStyle", ".uno:TitleParaStyle", ".uno:Heading1ParaStyle",
                ".uno:Heading2ParaStyle", ".uno:QuoteParaStyle"),
        ]),
        ("Insert", "Insert", 5, [
            big(".uno:InsertGraphic", ".uno:InsertTable|menu"),
        ]),
        ("Editing", "Editing", 2, [
            rows([FIND + "|text"], [".uno:SearchDialog|text"]),
        ]),
    ]),
    ("Insert", "Insert", [
        ("Tables", "Tables", 1, [
            big(".uno:InsertTable|menu"),
        ]),
        ("Illustrations", "Illustrations", 1, [
            big(".uno:InsertGraphic", ".uno:InsertObjectChart", ".uno:BasicShapes|menu"),
        ]),
        ("Links", "Links", 2, [
            big(".uno:HyperlinkDialog"),
            rows([".uno:InsertBookmark|text"], [".uno:InsertReferenceField|text"]),
        ]),
        ("Comments", "Comments", 2, [
            big(".uno:InsertAnnotation"),
        ]),
        ("HeaderFooter", "Header and footer", 2, [
            rows([".uno:InsertPageHeader|text"], [".uno:InsertPageFooter|text"],
                 [".uno:PageNumberWizard|text"]),
        ]),
        ("TextObjects", "Text", 3, [
            big(".uno:Text"),
            rows([".uno:InsertFieldCtrl|menu|text"], [".uno:InsertSection|text"]),
        ]),
        ("Symbols", "Symbols", 3, [
            big(".uno:CharmapControl|menu", ".uno:InsertObjectStarMath"),
        ]),
    ]),
    ("Layout", "Layout", [
        ("PageSetup", "Page setup", 1, [
            big(".uno:PageMargin|menu", ".uno:Orientation|menu", ".uno:AttributePageSize|menu",
                ".uno:PageColumnType|menu"),
        ]),
        ("Breaks", "Breaks", 2, [
            rows([".uno:InsertPagebreak|text"], [".uno:InsertColumnBreak|text"],
                 [".uno:InsertBreak|text"]),
        ]),
        ("PageStyle", "Page", 2, [
            big(".uno:PageDialog"),
            rows([".uno:TitlePageDialog|text"], [".uno:FormatColumns|text"]),
            rows([".uno:LineNumberingDialog|text"], [".uno:Hyphenate|text"]),
        ]),
        ("Arrange", "Arrange", 3, [
            big(".uno:WrapMenu|menu"),
            rows([".uno:BringToFront|text"], [".uno:SendToBack|text"],
                 [".uno:ObjectAlign|menu|text"]),
        ]),
    ]),
    ("References", "References", [
        ("Contents", "Table of contents", 1, [
            big(".uno:InsertMultiIndex"),
            rows([".uno:UpdateCurIndex|text"], [".uno:UpdateAllIndexes|text"]),
        ]),
        ("Footnotes", "Footnotes", 1, [
            big(".uno:InsertFootnote"),
            rows([".uno:InsertEndnote|text"], [".uno:FootnoteDialog|text"]),
        ]),
        ("Citations", "Citations", 2, [
            big(".uno:InsertAuthoritiesEntry"),
        ]),
        ("Captions", "Captions", 2, [
            big(".uno:InsertCaptionDialog"),
            rows([".uno:InsertReferenceField|text"], [".uno:InsertBookmark|text"]),
        ]),
        ("Index", "Index", 3, [
            big(".uno:InsertIndexesEntry"),
        ]),
        ("Update", "Update", 2, [
            big(".uno:UpdateAll"),
        ]),
    ]),
    ("Review", "Review", [
        ("Proofing", "Proofing", 1, [
            big(".uno:SpellingAndGrammarDialog"),
            rows([".uno:SpellOnline|text"], [".uno:ThesaurusDialog|text"],
                 [".uno:WordCountDialog|text"]),
        ]),
        ("Comments", "Comments", 1, [
            big(".uno:InsertAnnotation"),
            rows([".uno:ShowAnnotations|text"], [".uno:DeleteAllNotes|text"]),
        ]),
        ("Tracking", "Track changes", 1, [
            big(".uno:TrackChanges"),
            rows([".uno:ShowTrackedChanges|text"], [".uno:AcceptTrackedChanges|text"]),
        ]),
        ("Changes", "Changes", 2, [
            rows([".uno:AcceptTrackedChange|text"], [".uno:RejectTrackedChange|text"]),
            rows([".uno:PreviousTrackedChange|text"], [".uno:NextTrackedChange|text"]),
        ]),
        ("Compare", "Compare", 3, [
            rows([".uno:CompareDocuments|text"], [".uno:ProtectTraceChangeMode|text"]),
        ]),
    ]),
    ("View", "View", [
        ("Views", "Views", 1, [
            big(".uno:PrintLayout", ".uno:BrowseView"),
        ]),
        ("Show", "Show", 1, [
            rows([".uno:Ruler|text"], [".uno:ControlCodes|text"], [".uno:ShowWhitespace|text"]),
            rows([".uno:Navigator|text"], [".uno:Sidebar|text"]),
        ]),
        ("Zoom", "Zoom", 1, [
            big(".uno:Zoom", ".uno:Zoom100Percent"),
            rows([".uno:ZoomPage|text"], [".uno:ZoomPageWidth|text"]),
        ]),
        ("Window", "Window", 2, [
            big(".uno:FullScreen"),
        ]),
    ]),
]

CALC = [
    ("Home", "Home", [
        CLIPBOARD,
        ("Text", "Text", 1, [
            rows([".uno:CharFontName|w=140", ".uno:FontHeight|w=70"],
                 [".uno:Bold", ".uno:Italic", ".uno:Underline|menu",
                  ".uno:Color|menu", ".uno:BackgroundColor|menu"]),
        ]),
        ("Alignment", "Alignment", 2, [
            rows([".uno:AlignLeft", ".uno:AlignHorizontalCenter", ".uno:AlignRight"],
                 [".uno:WrapText", ".uno:ToggleMergeCells"]),
        ]),
        ("Number", "Number", 2, [
            rows([".uno:NumberFormatType"],
                 [".uno:NumberFormatCurrency|menu", ".uno:NumberFormatPercent",
                  ".uno:NumberFormatIncDecimals", ".uno:NumberFormatDecDecimals"]),
        ]),
        ("Cells", "Cells", 3, [
            big(".uno:InsertCell", ".uno:FormatCellDialog"),
        ]),
        ("Data", "Data", 3, [
            big(".uno:DataSort", ".uno:DataFilterAutoFilter", ".uno:InsertObjectChart"),
        ]),
        ("Editing", "Editing", 2, [
            rows([".uno:AutoSum|menu|text"], [FIND + "|text"]),
        ]),
    ]),
    ("Insert", "Insert", [
        ("Tables", "Tables", 1, [
            big(".uno:DataDataPilotRun"),
        ]),
        ("Illustrations", "Illustrations", 1, [
            big(".uno:InsertGraphic", ".uno:InsertObjectChart", ".uno:BasicShapes|menu"),
            rows([".uno:InsertSparkline|text"]),
        ]),
        ("Links", "Links", 2, [
            big(".uno:HyperlinkDialog"),
        ]),
        ("Comments", "Comments", 2, [
            big(".uno:InsertAnnotation"),
        ]),
        ("TextObjects", "Text", 3, [
            big(".uno:DrawText"),
            rows([".uno:EditHeaderAndFooter|text"], [".uno:CharmapControl|menu|text"],
                 [".uno:InsertObjectStarMath|text"]),
        ]),
        ("Functions", "Functions", 2, [
            big(".uno:FunctionDialog"),
            rows([".uno:InsertName|text"], [".uno:DefineName|text"]),
        ]),
    ]),
    ("Layout", "Layout", [
        ("PageSetup", "Page setup", 1, [
            big(".uno:PageFormatDialog", ".uno:Margins|menu"),
            rows([".uno:EditHeaderAndFooter|text"]),
        ]),
        ("PrintArea", "Print area", 1, [
            rows([".uno:DefinePrintArea|text"], [".uno:EditPrintArea|text"],
                 [".uno:DeletePrintArea|text"]),
        ]),
        ("Breaks", "Breaks", 2, [
            rows([".uno:InsertRowBreak|text"], [".uno:InsertColumnBreak|text"]),
        ]),
        ("Size", "Cell size", 2, [
            rows([".uno:RowHeight|text"], [".uno:SetOptimalRowHeight|text"]),
            rows([".uno:ColumnWidth|text"], [".uno:SetOptimalColumnWidth|text"]),
        ]),
        ("Styles", "Styles", 3, [
            big(".uno:ConditionalFormatMenu|menu", ".uno:AutoFormat"),
        ]),
    ]),
    ("Data", "Data", [
        ("SortFilter", "Sort and filter", 1, [
            rows([".uno:SortAscending"], [".uno:SortDescending"]),
            big(".uno:DataSort", ".uno:DataFilterAutoFilter"),
            rows([".uno:DataFilterStandardFilter|text"], [".uno:DataFilterRemoveFilter|text"]),
        ]),
        ("Tools", "Data tools", 1, [
            big(".uno:TextToColumns", ".uno:HandleDuplicateRecords", ".uno:Validation"),
        ]),
        ("Outline", "Outline", 2, [
            rows([".uno:Group|text"], [".uno:Ungroup|text"]),
            rows([".uno:ShowDetail|text"], [".uno:HideDetail|text"]),
        ]),
        ("Analysis", "Analysis", 2, [
            rows([".uno:DataSubTotals|text"], [".uno:DataConsolidate|text"]),
            rows([".uno:GoalSeekDialog|text"], [".uno:SolverDialog|text"]),
        ]),
        ("Ranges", "Ranges", 3, [
            rows([".uno:DefineDBName|text"], [".uno:DataAreaRefresh|text"]),
        ]),
    ]),
    ("Review", "Review", [
        ("Proofing", "Proofing", 1, [
            big(".uno:SpellDialog"),
            rows([".uno:SpellOnline|text"], [".uno:ThesaurusDialog|text"]),
        ]),
        ("Comments", "Comments", 1, [
            big(".uno:InsertAnnotation"),
            rows([".uno:ShowAllNotes|text"], [".uno:HideAllNotes|text"],
                 [".uno:DeleteAllNotes|text"]),
        ]),
        ("Tracking", "Track changes", 1, [
            big(".uno:TraceChangeMode", ".uno:AcceptChanges"),
        ]),
        ("Compare", "Compare", 3, [
            big(".uno:CompareDocuments"),
        ]),
        ("Protect", "Protect", 2, [
            big(".uno:Protect", ".uno:ToolProtectionDocument"),
        ]),
    ]),
    ("View", "View", [
        ("Views", "Views", 1, [
            big(".uno:NormalViewMode", ".uno:PagebreakMode"),
        ]),
        ("Show", "Show", 1, [
            rows([".uno:InputLineVisible|text"], [".uno:ViewRowColumnHeaders|text"],
                 [".uno:ToggleSheetGrid|text"]),
            rows([".uno:ViewValueHighlighting|text"], [".uno:ToggleFormula|text"],
                 [".uno:Sidebar|text"]),
        ]),
        ("Zoom", "Zoom", 1, [
            big(".uno:Zoom", ".uno:Zoom100Percent"),
            rows([".uno:ZoomPlus|text"], [".uno:ZoomMinus|text"]),
        ]),
        ("Window", "Window", 2, [
            big(".uno:FreezePanes|menu", ".uno:SplitWindow", ".uno:FullScreen"),
        ]),
    ]),
    ("Sheet", "Sheet", [
        ("Sheets", "Sheets", 1, [
            big(".uno:Insert", ".uno:Remove"),
            rows([".uno:RenameTable|text"], [".uno:Move|text"], [".uno:SetTabBgColor|text"]),
            rows([".uno:Hide|text"], [".uno:Show|text"], [".uno:InsertSheetFromFile|text"]),
        ]),
        ("Rows", "Rows and columns", 1, [
            rows([".uno:InsertRowsBefore|text"], [".uno:InsertRowsAfter|text"],
                 [".uno:DeleteRows|text"]),
            rows([".uno:InsertColumnsBefore|text"], [".uno:InsertColumnsAfter|text"],
                 [".uno:DeleteColumns|text"]),
        ]),
        ("SheetCells", "Cells", 2, [
            big(".uno:InsertCell", ".uno:DeleteCell"),
            rows([".uno:FillDown|text"], [".uno:Delete|text"]),
        ]),
    ]),
]

# Short ribbon names, in sentence case and UK English. Menus keep
# LibreOffice's own wording: see labels().
LABELS = {
    ".uno:Paste": "Paste",
    ".uno:Cut": "Cut",
    ".uno:Copy": "Copy",
    ".uno:Color": "Font colour",
    ".uno:CharBackColor": "Highlight colour",
    ".uno:BackgroundColor": "Fill colour",
    ".uno:Strikeout": "Strikethrough",
    ".uno:DefaultBullet": "Bullets",
    ".uno:DefaultNumbering": "Numbering",
    ".uno:IncrementIndent": "Increase indent",
    ".uno:DecrementIndent": "Decrease indent",
    ".uno:LineSpacing": "Line spacing",
    ".uno:LeftPara": "Align left",
    ".uno:CenterPara": "Centre",
    ".uno:RightPara": "Align right",
    ".uno:JustifyPara": "Justify",
    ".uno:TextBodyParaStyle": "Body text",
    ".uno:TitleParaStyle": "Title",
    ".uno:Heading1ParaStyle": "Heading 1",
    ".uno:Heading2ParaStyle": "Heading 2",
    ".uno:QuoteParaStyle": "Quote",
    ".uno:InsertGraphic": "Picture",
    ".uno:InsertTable": "Table",
    FIND: "Find",
    ".uno:SearchDialog": "Replace",
    ".uno:CommandPopup": "Find a command",
    ".uno:InsertPagebreak": "Page break",
    ".uno:InsertObjectChart": "Chart",
    ".uno:BasicShapes": "Shapes",
    ".uno:HyperlinkDialog": "Link",
    ".uno:InsertBookmark": "Bookmark",
    ".uno:InsertReferenceField": "Cross-reference",
    ".uno:InsertAnnotation": "Comment",
    ".uno:InsertPageHeader": "Header",
    ".uno:InsertPageFooter": "Footer",
    ".uno:PageNumberWizard": "Page number",
    ".uno:Text": "Text box",
    ".uno:DrawText": "Text box",
    ".uno:InsertFieldCtrl": "Field",
    ".uno:InsertSection": "Section",
    ".uno:CharmapControl": "Symbol",
    ".uno:InsertObjectStarMath": "Equation",
    ".uno:PageMargin": "Margins",
    ".uno:Orientation": "Orientation",
    ".uno:AttributePageSize": "Size",
    ".uno:PageColumnType": "Columns",
    ".uno:InsertColumnBreak": "Column break",
    ".uno:InsertBreak": "More breaks",
    ".uno:Hyphenate": "Hyphenation",
    ".uno:LineNumberingDialog": "Line numbers",
    ".uno:PageDialog": "Page setup",
    ".uno:TitlePageDialog": "Title page",
    ".uno:FormatColumns": "Column settings",
    ".uno:WrapMenu": "Wrap text",
    ".uno:BringToFront": "Bring to front",
    ".uno:SendToBack": "Send to back",
    ".uno:ObjectAlign": "Align",
    ".uno:InsertMultiIndex": "Table of contents",
    ".uno:UpdateCurIndex": "Update table",
    ".uno:UpdateAllIndexes": "Update all tables",
    ".uno:InsertFootnote": "Footnote",
    ".uno:InsertEndnote": "Endnote",
    ".uno:FootnoteDialog": "Note settings",
    ".uno:InsertAuthoritiesEntry": "Citation",
    ".uno:InsertCaptionDialog": "Caption",
    ".uno:InsertIndexesEntry": "Index entry",
    ".uno:UpdateAll": "Update all",
    ".uno:SpellingAndGrammarDialog": "Spelling",
    ".uno:SpellDialog": "Spelling",
    ".uno:SpellOnline": "Check as you type",
    ".uno:ThesaurusDialog": "Thesaurus",
    ".uno:WordCountDialog": "Word count",
    ".uno:ShowAnnotations": "Show comments",
    ".uno:DeleteAllNotes": "Delete all comments",
    ".uno:TrackChanges": "Track changes",
    ".uno:TraceChangeMode": "Track changes",
    ".uno:ShowTrackedChanges": "Show changes",
    ".uno:AcceptTrackedChanges": "Manage changes",
    ".uno:AcceptChanges": "Manage changes",
    ".uno:AcceptTrackedChange": "Accept",
    ".uno:RejectTrackedChange": "Reject",
    ".uno:PreviousTrackedChange": "Previous",
    ".uno:NextTrackedChange": "Next",
    ".uno:CompareDocuments": "Compare",
    ".uno:ProtectTraceChangeMode": "Protect changes",
    ".uno:PrintLayout": "Print layout",
    ".uno:BrowseView": "Web layout",
    ".uno:Ruler": "Ruler",
    ".uno:ControlCodes": "Formatting marks",
    ".uno:ShowWhitespace": "Page gaps",
    ".uno:Navigator": "Navigator",
    ".uno:Sidebar": "Sidebar",
    ".uno:Zoom": "Zoom",
    ".uno:Zoom100Percent": "100%",
    ".uno:ZoomPage": "One page",
    ".uno:ZoomPageWidth": "Page width",
    ".uno:ZoomPlus": "Zoom in",
    ".uno:ZoomMinus": "Zoom out",
    ".uno:FullScreen": "Full screen",
    ".uno:AlignLeft": "Align left",
    ".uno:AlignHorizontalCenter": "Centre",
    ".uno:AlignRight": "Align right",
    ".uno:WrapText": "Wrap text",
    ".uno:ToggleMergeCells": "Merge and centre",
    ".uno:NumberFormatCurrency": "Currency",
    ".uno:NumberFormatPercent": "Percent",
    ".uno:NumberFormatIncDecimals": "More decimals",
    ".uno:NumberFormatDecDecimals": "Fewer decimals",
    ".uno:InsertCell": "Insert",
    ".uno:FormatCellDialog": "Format",
    ".uno:DataSort": "Sort",
    ".uno:DataFilterAutoFilter": "Filter",
    ".uno:AutoSum": "AutoSum",
    ".uno:DataDataPilotRun": "Pivot table",
    ".uno:InsertSparkline": "Sparkline",
    ".uno:EditHeaderAndFooter": "Header and footer",
    ".uno:FunctionDialog": "Function",
    ".uno:InsertName": "Use a name",
    ".uno:DefineName": "Manage names",
    ".uno:PageFormatDialog": "Page setup",
    ".uno:Margins": "Margins",
    ".uno:DefinePrintArea": "Set print area",
    ".uno:EditPrintArea": "Edit print area",
    ".uno:DeletePrintArea": "Clear print area",
    ".uno:InsertRowBreak": "Row break",
    ".uno:RowHeight": "Row height",
    ".uno:SetOptimalRowHeight": "Fit rows",
    ".uno:ColumnWidth": "Column width",
    ".uno:SetOptimalColumnWidth": "Fit columns",
    ".uno:ConditionalFormatMenu": "Conditional",
    ".uno:AutoFormat": "Table styles",
    ".uno:SortAscending": "Sort A to Z",
    ".uno:SortDescending": "Sort Z to A",
    ".uno:DataFilterStandardFilter": "Advanced filter",
    ".uno:DataFilterRemoveFilter": "Clear filter",
    ".uno:TextToColumns": "Text to columns",
    ".uno:HandleDuplicateRecords": "Duplicates",
    ".uno:Validation": "Validation",
    ".uno:Group": "Group",
    ".uno:Ungroup": "Ungroup",
    ".uno:ShowDetail": "Show detail",
    ".uno:HideDetail": "Hide detail",
    ".uno:DataSubTotals": "Subtotals",
    ".uno:DataConsolidate": "Consolidate",
    ".uno:GoalSeekDialog": "Goal seek",
    ".uno:SolverDialog": "Solver",
    ".uno:DefineDBName": "Define range",
    ".uno:DataAreaRefresh": "Refresh range",
    ".uno:ShowAllNotes": "Show all comments",
    ".uno:HideAllNotes": "Hide all comments",
    ".uno:Protect": "Protect sheet",
    ".uno:ToolProtectionDocument": "Protect workbook",
    ".uno:NormalViewMode": "Normal",
    ".uno:PagebreakMode": "Page break preview",
    ".uno:InputLineVisible": "Formula bar",
    ".uno:ViewRowColumnHeaders": "Headings",
    ".uno:ToggleSheetGrid": "Gridlines",
    ".uno:ViewValueHighlighting": "Highlight values",
    ".uno:ToggleFormula": "Show formulas",
    ".uno:FreezePanes": "Freeze panes",
    ".uno:SplitWindow": "Split",
    ".uno:Insert": "Insert sheet",
    ".uno:Remove": "Delete sheet",
    ".uno:RenameTable": "Rename",
    ".uno:Move": "Move or copy",
    ".uno:SetTabBgColor": "Tab colour",
    ".uno:Hide": "Hide sheet",
    ".uno:Show": "Unhide sheet",
    ".uno:InsertSheetFromFile": "Sheet from file",
    ".uno:InsertRowsBefore": "Rows above",
    ".uno:InsertRowsAfter": "Rows below",
    ".uno:DeleteRows": "Delete rows",
    ".uno:InsertColumnsBefore": "Columns before",
    ".uno:InsertColumnsAfter": "Columns after",
    ".uno:DeleteColumns": "Delete columns",
    ".uno:DeleteCell": "Delete",
    ".uno:FillDown": "Fill down",
    ".uno:Delete": "Clear",
}


class Writer:
    def __init__(self, module):
        self.module = module
        self.out = []
        self.ids = set()

    def uid(self, base):
        base = base.replace(" ", "")
        n, i = base, 1
        while n in self.ids:
            i += 1
            n = "%s%d" % (base, i)
        self.ids.add(n)
        return n

    def w(self, depth, text):
        self.out.append("  " * depth + text)

    def props(self, d, props):
        for k, v in props:
            self.w(d, '<property name="%s">%s</property>' % (k, escape(str(v))))

    def packing(self, d, props):
        self.w(d, "<packing>")
        self.props(d + 1, props)
        self.w(d, "</packing>")

    def box_child(self, d, pos, fn, expand=False, fill=True, padding=None, end=False):
        self.w(d, "<child>")
        fn(d + 1)
        p = [("expand", expand), ("fill", fill)]
        if padding is not None:
            p.append(("padding", padding))
        if end:
            p.append(("pack_type", "end"))
        p.append(("position", pos))
        self.packing(d + 1, [(k, str(v) if not isinstance(v, bool) else ("True" if v else "False"))
                             for k, v in p])
        self.w(d, "</child>")

    def toolbox(self, d, items, prefix, style, large=False):
        tid = self.uid(prefix + "-tb")
        self.w(d, '<object class="sfxlo-NotebookbarToolBox" id="%s">' % tid)
        p = [("visible", "True"), ("can_focus", "True"), ("valign", "center")]
        if style:
            p.append(("toolbar_style", style))
        p.append(("show_arrow", "False"))
        if large:
            p.append(("icon_size", "3"))
        self.props(d + 1, p)
        for it in items:
            parts = it.split("|")
            cmd = parts[0]
            cls = "GtkMenuToolButton" if "menu" in parts[1:] else "GtkToolButton"
            name = cmd.split(":")[-1].split("?")[0]
            self.w(d + 1, "<child>")
            self.w(d + 2, '<object class="%s" id="%s">' % (cls, self.uid(prefix + "-" + name)))
            props = [("visible", "True"), ("action_name", cmd)]
            props += [("width_request", x[2:]) for x in parts[1:] if x.startswith("w=")]
            self.props(d + 3, props)
            self.w(d + 2, "</object>")
            self.packing(d + 2, [("expand", "False"), ("homogeneous", "False")])
            self.w(d + 1, "</child>")
        self.w(d, "</object>")

    def column(self, d, col, prefix):
        kind, body = col
        if kind == "big":
            self.toolbox(d, body, prefix, "both", large=True)
            return
        bid = self.uid(prefix + "-rows")
        self.w(d, '<object class="GtkBox" id="%s">' % bid)
        self.props(d + 1, [("visible", "True"), ("can_focus", "False"), ("valign", "center"),
                           ("orientation", "vertical"), ("spacing", "2")])
        for i, row in enumerate(body):
            texty = any("text" in it.split("|")[1:] for it in row)
            self.box_child(d + 1, i, lambda dd, row=row, texty=texty:
                           self.toolbox(dd, row, prefix, "both-horiz" if texty else None))
        self.w(d, "</object>")

    def group(self, d, tab, grp, first):
        gid, label, prio, cols = grp
        prefix = "%s-%s" % (tab, gid)
        self.w(d, '<object class="VclOptionalBox" id="%s">' % self.uid(prefix))
        self.props(d + 1, [("visible", "True"), ("can_focus", "False"), ("vexpand", "True")])
        pos = 0
        if not first:
            def sep(dd):
                self.w(dd, '<object class="GtkSeparator" id="%s">' % self.uid(prefix + "-sep"))
                self.props(dd + 1, [("visible", "True"), ("can_focus", "False"),
                                    ("margin_top", "4"), ("margin_bottom", "4"),
                                    ("orientation", "vertical")])
                self.w(dd, "</object>")
            self.box_child(d + 1, pos, sep, padding=3)
            pos += 1

        def stack(dd):
            self.w(dd, '<object class="GtkBox" id="%s">' % self.uid(prefix + "-stack"))
            self.props(dd + 1, [("visible", "True"), ("can_focus", "False"),
                                ("orientation", "vertical"), ("spacing", "2")])

            def content(d3):
                self.w(d3, '<object class="GtkBox" id="%s">' % self.uid(prefix + "-content"))
                self.props(d3 + 1, [("visible", "True"), ("can_focus", "False"),
                                    ("valign", "center"), ("vexpand", "True"), ("spacing", "2")])
                for i, col in enumerate(cols):
                    self.box_child(d3 + 1, i, lambda d4, col=col: self.column(d4, col, prefix))
                self.w(d3, "</object>")
            self.box_child(dd + 1, 0, content, expand=True)

            def lab(d3):
                self.w(d3, '<object class="GtkLabel" id="%s">' % self.uid(prefix + "-label"))
                self.props(d3 + 1, [("visible", "True"), ("can_focus", "False")])
                self.w(d3 + 1, '<property name="label" translatable="yes">%s</property>' % escape(label))
                self.props(d3 + 1, [("halign", "center")])
                self.w(d3 + 1, "<attributes>")
                self.w(d3 + 2, '<attribute name="scale" value="0.85"/>')
                self.w(d3 + 1, "</attributes>")
                self.w(d3, "</object>")
            self.box_child(dd + 1, 1, lab)
            self.w(dd, "</object>")
        self.box_child(d + 1, pos, stack)
        self.w(d + 1, "<style>")
        # LibreOffice folds the lowest priority value first.
        self.w(d + 2, '<class name="priority-%d"/>' % (10 - prio))
        self.w(d + 1, "</style>")
        self.w(d, "</object>")

    def tab(self, d, pos, tab):
        tid, label, groups = tab

        def page(dd):
            self.w(dd, '<object class="GtkBox" id="%s Tab">' % tid)
            self.props(dd + 1, [("visible", "True"), ("can_focus", "False"), ("vexpand", "True")])

            def merged(d3):
                self.w(d3, '<object class="sfxlo-PriorityMergedHBox" id="%s">' % tid)
                self.props(d3 + 1, [("visible", "True"), ("can_focus", "False"),
                                    ("margin_start", "2"), ("margin_top", "4"),
                                    ("margin_bottom", "2"), ("vexpand", "True")])
                for i, g in enumerate(groups):
                    self.box_child(d3 + 1, i, lambda d4, g=g, i=i: self.group(d4, tid, g, i == 0))
                self.w(d3, "</object>")
            # Expanded so the merged box gets the page's whole width to
            # decide what fits; it asks for almost none itself.
            self.box_child(dd + 1, 0, merged, expand=True)
            self.w(dd, "</object>")

        self.w(d, "<child>")
        page(d + 1)
        self.packing(d + 1, [("position", str(pos))])
        self.w(d, "</child>")
        self.w(d, '<child type="tab">')
        self.w(d + 1, '<object class="GtkLabel" id="%sLabel">' % tid)
        self.props(d + 2, [("visible", "True"), ("can_focus", "False")])
        self.w(d + 2, '<property name="label" translatable="yes">%s</property>' % escape(label))
        self.w(d + 1, "</object>")
        self.packing(d + 1, [("position", str(pos)), ("tab_fill", "False")])
        self.w(d, "</child>")

    def file_menu(self, d):
        self.w(d, '<object class="GtkMenu" id="FileMenu">')
        self.props(d + 1, [("visible", "True"), ("can_focus", "False")])
        for i, cmd in enumerate(FILE_MENU[self.module]):
            self.w(d + 1, "<child>")
            if cmd == "-":
                self.w(d + 2, '<object class="GtkSeparatorMenuItem" id="%s">' % self.uid("File-sep"))
                self.props(d + 3, [("visible", "True"), ("can_focus", "False")])
            else:
                self.w(d + 2, '<object class="GtkMenuItem" id="%s">' % self.uid("File-" + cmd.split(":")[-1]))
                self.props(d + 3, [("visible", "True"), ("can_focus", "False"), ("action_name", cmd)])
            self.w(d + 2, "</object>")
            self.w(d + 1, "</child>")
        self.w(d, "</object>")

    def build(self, tabs):
        self.w(0, '<?xml version="1.0" encoding="UTF-8"?>')
        self.w(0, "<!-- Generated by packages/wildos-office/tools/gen-ribbons.py; edit that, not this. -->")
        self.w(0, '<interface domain="%s">' % ("sw" if self.module == "swriter" else "sc"))
        self.w(1, '<requires lib="gtk+" version="3.20"/>')
        self.w(1, '<requires lib="LibreOffice" version="1.0"/>')
        self.file_menu(1)
        self.w(1, '<object class="GtkGrid" id="NotebookBar">')
        self.props(2, [("visible", "True"), ("can_focus", "False")])
        self.w(2, "<child>")
        self.w(3, '<object class="GtkBox" id="box">')
        self.props(4, [("visible", "True"), ("can_focus", "False"), ("hexpand", "True"),
                       ("spacing", "4")])

        # File sits left of the tabs and Find a command right of them, as on
        # the boards. Neither can live in the tab row itself, so both are
        # siblings of the notebook, pinned to its top edge.
        def file_button(d):
            self.w(d, '<object class="svtlo-ManagedMenuButton" id="FileButton:FileMenu">')
            self.props(d + 1, [("visible", "True"), ("can_focus", "True"),
                               ("has_default", "True"), ("valign", "start"),
                               ("margin_start", "4"), ("margin_top", "6")])
            self.w(d + 1, '<property name="label" translatable="yes">File</property>')
            self.w(d, "</object>")
        self.box_child(4, 0, file_button)

        def notebook(d):
            self.w(d, '<object class="GtkNotebook" id="Tabs">')
            # A notebook asks for the width of its widest page, which would
            # make the whole bar wider than a small window. The fixed request
            # lets it shrink, and each page then folds groups to fit.
            self.props(d + 1, [("visible", "True"), ("can_focus", "True"), ("hexpand", "True"),
                               ("width_request", "400"), ("show_border", "False")])
            for i, t in enumerate(tabs):
                self.tab(d + 1, i, t)
            self.w(d, "</object>")
        self.box_child(4, 1, notebook, expand=True)

        def find_command(d):
            self.w(d, '<object class="GtkButton" id="FindCommand">')
            self.props(d + 1, [("visible", "True"), ("can_focus", "True"),
                               ("receives_default", "False"), ("valign", "start"),
                               ("width_request", "170"), ("margin_end", "4"), ("margin_top", "6"),
                               ("action_name", ".uno:CommandPopup"),
                               ("xalign", "0")])
            self.w(d + 1, '<property name="label" translatable="yes">Find a command</property>')
            self.w(d, "</object>")
        self.box_child(4, 2, find_command)
        self.w(3, "</object>")
        self.packing(3, [("left_attach", "0"), ("top_attach", "0")])
        self.w(2, "</child>")
        self.w(1, "</object>")
        self.w(0, "</interface>")
        return "\n".join(self.out) + "\n"


def commands(tabs):
    seen = []
    for _, _, groups in tabs:
        for _, _, _, cols in groups:
            for kind, body in cols:
                for it in (body if kind == "big" else [i for r in body for i in r]):
                    c = it.split("|")[0]
                    if c not in seen:
                        seen.append(c)
    return seen


def labels():
    """Registry overrides that give ribbon buttons their Wild OS names.

    A toolbar shows a command's Label; menus and tooltips prefer its
    ContextLabel. So the short name goes in Label and LibreOffice's original
    Label moves to ContextLabel where the command has none, which keeps
    menus as they were. Run against LibreOffice's own registry to get the
    originals: tools/gen-ribbons.py --labels main.xcd writer.xcd calc.xcd
    """
    from xml.etree import ElementTree as ET
    O = "{http://openoffice.org/2001/registry}"
    comps = {"GenericCommands": {}, "WriterCommands": {}, "CalcCommands": {}}
    for path in sys.argv[2:]:
        for cd in ET.parse(path).getroot():
            comp = comps.get(cd.get(O + "name"))
            if comp is None:
                continue
            # Most commands sit in the Commands set; the paragraph style
            # shortcuts sit in Popups.
            for st in cd.iter("node"):
                if st.get(O + "name") not in ("Commands", "Popups"):
                    continue
                for n in st.findall("node"):
                    d = comp.setdefault(n.get(O + "name"), {"sets": []})
                    d["sets"].append(st.get(O + "name"))
                    for p in n.findall("prop"):
                        v = p.find("value")
                        if st.get(O + "name") == "Commands" or p.get(O + "name") not in d:
                            d[p.get(O + "name")] = v.text if v is not None else None
    used = set(commands(WRITER)) | set(commands(CALC)) | {".uno:CommandPopup"}
    out = []
    for comp, table in comps.items():
        # A command with a TargetURL may show its target's label, so the
        # target gets the same name.
        wanted = {}
        for cmd in sorted(used):
            if cmd in LABELS and cmd in table:
                wanted[cmd] = LABELS[cmd]
                target = table[cmd].get("TargetURL")
                if target in table:
                    wanted[target] = LABELS[cmd]
        sets = {}
        for cmd, label in sorted(wanted.items()):
            orig = table[cmd]
            if (orig.get("Label") or "").replace("~", "") == label:
                continue
            for st in orig["sets"]:
                nodes = sets.setdefault(st, [])
                nodes.append('        <node oor:name="%s">' % escape(cmd, {'"': "&quot;"}))
                nodes.append('          <prop oor:name="Label"><value xml:lang="en-US">%s</value>'
                             '<value xml:lang="en-GB">%s</value></prop>' % ((escape(label),) * 2))
                if not orig.get("ContextLabel") and orig.get("Label"):
                    nodes.append('          <prop oor:name="ContextLabel"><value xml:lang="en-US">%s</value></prop>'
                                 % escape(orig["Label"]))
                nodes.append("        </node>")
        if sets:
            out.append('  <oor:component-data oor:package="org.openoffice.Office.UI" oor:name="%s">' % comp)
            out.append('    <node oor:name="UserInterface">')
            for name, nodes in sorted(sets.items()):
                out.append('      <node oor:name="%s">' % name)
                out.extend(nodes)
                out.append("      </node>")
            out.append("    </node>")
            out.append("  </oor:component-data>")
    return "\n".join(out) + "\n"


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--labels":
        sys.stdout.write(labels())
        return
    for module, tabs in (("swriter", WRITER), ("scalc", CALC)):
        path = os.path.join(UI, module, "notebookbar_wildos.ui")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(Writer(module).build(tabs))
        print(path)


if __name__ == "__main__":
    main()
