#!/usr/bin/env python3
"""Builds the Write and Tables templates, their card thumbnails and the two
blank-document defaults.

Each template is written here as flat ODF (.fodt or .fods), so every style
and every line of example text can be read and changed in one place, then
LibreOffice turns it into the .ott or .ots that ships and renders its first
page for the start screen's card. Run it where LibreOffice, Carlito and
ImageMagick are installed (any Debian trixie with libreoffice-writer,
libreoffice-calc, fonts-crosextra-carlito and imagemagick):

    python3 make-templates.py

The results are committed, so package builds never need LibreOffice.
"""

import os
import shutil
import subprocess
import sys
import tempfile
from xml.sax.saxutils import escape

HERE = os.path.dirname(os.path.abspath(__file__))

INK = "#1f2633"
MUTED = "#5b6478"
BLUE = "#3550c8"
GREEN = "#1f8a5b"
RULE = "#d7dbe3"
TINT_BLUE = "#eef2fd"
TINT_GREEN = "#e8f6ee"

NS = (
    'xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" '
    'xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" '
    'xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" '
    'xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0" '
    'xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" '
    'xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0" '
    'xmlns:xlink="http://www.w3.org/1999/xlink" '
    'xmlns:dc="http://purl.org/dc/elements/1.1/" '
    'xmlns:meta="urn:oasis:names:tc:opendocument:xmlns:meta:1.0" '
    'xmlns:number="urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0" '
    'xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0" '
    'xmlns:config="urn:oasis:names:tc:opendocument:xmlns:config:1.0" '
    'xmlns:of="urn:oasis:names:tc:opendocument:xmlns:of:1.2" '
    'xmlns:loext="urn:org:documentfoundation:names:experimental:office:xmlns:loext:1.0" '
    'xmlns:calcext="urn:org:documentfoundation:names:experimental:calc:xmlns:calcext:1.0"'
)

FONTS = """<office:font-face-decls>
  <style:font-face style:name="Carlito" svg:font-family="Carlito" style:font-family-generic="swiss" style:font-pitch="variable"/>
</office:font-face-decls>"""

LANG = 'fo:language="en" fo:country="GB"'

# "10 November 2026", the way a UK letter dates itself.
DATE_LONG = """<number:date-style style:name="NDateLong">
  <number:day/><number:text> </number:text><number:month number:textual="true" number:style="long"/><number:text> </number:text><number:year number:style="long"/>
</number:date-style>"""


def meta(title):
    return f"""<office:meta>
  <dc:title>{escape(title)}</dc:title>
  <dc:language>en-GB</dc:language>
</office:meta>"""


# Writer

def writer_styles(accent):
    """The Write look (WriteDoc board): Carlito 11 in dark ink, a dark bold
    Title, Heading 1 and 2 in the accent, a small capitals Label above a
    title, and bullets in the accent. Everything maps onto a Word style of
    the same name, so .docx saves keep it."""
    return f"""<office:styles>
  {DATE_LONG}
  <style:default-style style:family="graphic">
    <style:graphic-properties svg:stroke-color="#3465a4" draw:fill-color="#729fcf" fo:wrap-option="no-wrap" draw:shadow-offset-x="0.3cm" draw:shadow-offset-y="0.3cm" style:flow-with-text="false"/>
    <style:paragraph-properties style:writing-mode="lr-tb"/>
    <style:text-properties style:font-name="Carlito" fo:font-size="11pt" {LANG}/>
  </style:default-style>
  <style:default-style style:family="paragraph">
    <style:paragraph-properties fo:orphans="2" fo:widows="2" style:tab-stop-distance="1.25cm" style:writing-mode="page"/>
    <style:text-properties style:font-name="Carlito" fo:font-size="11pt" {LANG} style:letter-kerning="true" fo:hyphenate="false"/>
  </style:default-style>
  <style:default-style style:family="table">
    <style:table-properties table:border-model="collapsing"/>
  </style:default-style>
  <style:default-style style:family="table-row">
    <style:table-row-properties fo:keep-together="auto"/>
  </style:default-style>
  <style:style style:name="Standard" style:family="paragraph" style:class="text">
    <style:paragraph-properties fo:margin-top="0cm" fo:margin-bottom="0.282cm" fo:line-height="115%"/>
    <style:text-properties fo:color="{INK}"/>
  </style:style>
  <style:style style:name="Text_20_body" style:display-name="Body Text" style:family="paragraph" style:parent-style-name="Standard" style:class="text"/>
  <style:style style:name="Heading" style:family="paragraph" style:parent-style-name="Standard" style:next-style-name="Text_20_body" style:class="text">
    <style:paragraph-properties fo:keep-with-next="always" fo:line-height="100%"/>
    <style:text-properties fo:font-weight="bold"/>
  </style:style>
  <style:style style:name="Title" style:family="paragraph" style:parent-style-name="Heading" style:next-style-name="Text_20_body" style:class="chapter">
    <style:paragraph-properties fo:margin-top="0cm" fo:margin-bottom="0.35cm"/>
    <style:text-properties fo:font-size="24pt" fo:color="{INK}"/>
  </style:style>
  <style:style style:name="Subtitle" style:family="paragraph" style:parent-style-name="Heading" style:next-style-name="Text_20_body" style:class="chapter">
    <style:paragraph-properties fo:margin-top="0cm" fo:margin-bottom="0.45cm"/>
    <style:text-properties fo:font-size="13pt" fo:font-weight="normal" fo:color="{MUTED}"/>
  </style:style>
  <style:style style:name="Label" style:family="paragraph" style:parent-style-name="Standard" style:next-style-name="Title" style:class="text">
    <style:paragraph-properties fo:margin-bottom="0.1cm" fo:keep-with-next="always"/>
    <style:text-properties fo:font-size="9pt" fo:font-weight="bold" fo:letter-spacing="0.06cm" fo:text-transform="uppercase" fo:color="{accent}"/>
  </style:style>
  <style:style style:name="Heading_20_1" style:display-name="Heading 1" style:family="paragraph" style:parent-style-name="Heading" style:next-style-name="Text_20_body" style:default-outline-level="1" style:class="text">
    <style:paragraph-properties fo:margin-top="0.55cm" fo:margin-bottom="0.2cm"/>
    <style:text-properties fo:font-size="15pt" fo:color="{accent}"/>
  </style:style>
  <style:style style:name="Heading_20_2" style:display-name="Heading 2" style:family="paragraph" style:parent-style-name="Heading" style:next-style-name="Text_20_body" style:default-outline-level="2" style:class="text">
    <style:paragraph-properties fo:margin-top="0.4cm" fo:margin-bottom="0.15cm"/>
    <style:text-properties fo:font-size="12.5pt" fo:color="{accent}"/>
  </style:style>
  <style:style style:name="Heading_20_3" style:display-name="Heading 3" style:family="paragraph" style:parent-style-name="Heading" style:next-style-name="Text_20_body" style:default-outline-level="3" style:class="text">
    <style:paragraph-properties fo:margin-top="0.3cm" fo:margin-bottom="0.1cm"/>
    <style:text-properties fo:font-size="11pt" fo:color="{INK}"/>
  </style:style>
  <style:style style:name="Quotations" style:display-name="Quote" style:family="paragraph" style:parent-style-name="Standard" style:class="html">
    <style:paragraph-properties fo:margin-left="0.5cm" fo:margin-right="0.5cm" fo:padding-left="0.35cm" fo:border-left="1.5pt solid {accent}" fo:border-right="none" fo:border-top="none" fo:border-bottom="none"/>
    <style:text-properties fo:font-style="italic" fo:color="{MUTED}"/>
  </style:style>
  <style:style style:name="List_20_Paragraph" style:display-name="List Paragraph" style:family="paragraph" style:parent-style-name="Standard" style:class="list">
    <style:paragraph-properties fo:margin-bottom="0.1cm" style:contextual-spacing="true"/>
  </style:style>
  <style:style style:name="Table_20_Contents" style:display-name="Table Contents" style:family="paragraph" style:parent-style-name="Standard" style:class="extra">
    <style:paragraph-properties fo:margin-bottom="0cm"/>
  </style:style>
  <style:style style:name="Table_20_Heading" style:display-name="Table Heading" style:family="paragraph" style:parent-style-name="Table_20_Contents" style:class="extra">
    <style:text-properties fo:font-weight="bold"/>
  </style:style>
  <style:style style:name="Footer" style:family="paragraph" style:parent-style-name="Standard" style:class="extra">
    <style:paragraph-properties fo:margin-bottom="0cm" fo:text-align="center"/>
    <style:text-properties fo:font-size="9pt" fo:color="{MUTED}"/>
  </style:style>
  <style:style style:name="Strong_20_Emphasis" style:display-name="Strong" style:family="text">
    <style:text-properties fo:font-weight="bold"/>
  </style:style>
  <text:outline-style style:name="Outline">
    {''.join(f'<text:outline-level-style text:level="{n}" style:num-format=""><style:list-level-properties text:list-level-position-and-space-mode="label-alignment"><style:list-level-label-alignment text:label-followed-by="listtab"/></style:list-level-properties></text:outline-level-style>' for n in range(1, 11))}
  </text:outline-style>
  <text:list-style style:name="Bullets">
    {''.join(f'<text:list-level-style-bullet text:level="{n}" text:bullet-char="{"•" if n % 2 else "◦"}"><style:list-level-properties text:list-level-position-and-space-mode="label-alignment"><style:list-level-label-alignment text:label-followed-by="listtab" text:list-tab-stop-position="{0.6 * n:.2f}cm" fo:text-indent="-0.6cm" fo:margin-left="{0.6 * n:.2f}cm"/></style:list-level-properties><style:text-properties fo:color="{accent}"/></text:list-level-style-bullet>' for n in range(1, 11))}
  </text:list-style>
  <text:list-style style:name="Numbering">
    {''.join(f'<text:list-level-style-number text:level="{n}" style:num-suffix="." style:num-format="{"1" if n % 2 else "a"}"><style:list-level-properties text:list-level-position-and-space-mode="label-alignment"><style:list-level-label-alignment text:label-followed-by="listtab" text:list-tab-stop-position="{0.65 * n:.2f}cm" fo:text-indent="-0.65cm" fo:margin-left="{0.65 * n:.2f}cm"/></style:list-level-properties><style:text-properties fo:color="{accent}" fo:font-weight="bold"/></text:list-level-style-number>' for n in range(1, 11))}
  </text:list-style>
  <text:notes-configuration text:note-class="footnote" style:num-format="1" text:start-value="0" text:footnotes-position="page" text:start-numbering-at="document"/>
  <text:linenumbering-configuration text:number-lines="false" text:offset="0.499cm" style:num-format="1" text:number-position="left" text:increment="5"/>
</office:styles>"""


def page_layout(margins="2.5cm", footer=None):
    foot = ""
    if footer is not None:
        foot = '<style:footer-style><style:header-footer-properties fo:min-height="0cm" fo:margin-top="0.4cm"/></style:footer-style>'
    return f"""<style:page-layout style:name="pm1">
    <style:page-layout-properties fo:page-width="21cm" fo:page-height="29.7cm" style:num-format="1" style:print-orientation="portrait" fo:margin-top="{margins}" fo:margin-bottom="{margins}" fo:margin-left="{margins}" fo:margin-right="{margins}" style:writing-mode="lr-tb"/>
    <style:header-style/>
    {foot}
  </style:page-layout>"""


def master(footer=None):
    if footer is None:
        return '<office:master-styles><style:master-page style:name="Standard" style:page-layout-name="pm1"/></office:master-styles>'
    return f"""<office:master-styles><style:master-page style:name="Standard" style:page-layout-name="pm1">
  <style:footer><text:p text:style-name="Footer">{footer}</text:p></style:footer>
</style:master-page></office:master-styles>"""


def inline(text):
    """Plain text with **bold**, tabs and line breaks."""
    out = []
    bold = False
    for i, part in enumerate(text.split("**")):
        part = escape(part).replace("\t", "<text:tab/>").replace("\n", "<text:line-break/>")
        if bold and part:
            out.append(f'<text:span text:style-name="Strong_20_Emphasis">{part}</text:span>')
        else:
            out.append(part)
        bold = not bold
    return "".join(out)


def p(text="", style="Text_20_body"):
    return f'<text:p text:style-name="{style}">{inline(text)}</text:p>'


def h(level, text):
    return f'<text:h text:style-name="Heading_20_{level}" text:outline-level="{level}">{inline(text)}</text:h>'


def bullets(items, numbered=False):
    style = "Numbering" if numbered else "Bullets"
    rows = "".join(f'<text:list-item><text:p text:style-name="List_20_Paragraph">{inline(i)}</text:p></text:list-item>' for i in items)
    return f'<text:list text:style-name="{style}">{rows}</text:list>'


class WriterTables:
    """Tables in the Write look: a tinted header row with an accent rule
    under it, and a hairline under every other row."""

    def __init__(self, accent, tint):
        self.accent = accent
        self.tint = tint
        self.count = 0
        self.styles = []

    def automatic(self):
        return f"""<style:style style:name="CellHead" style:family="table-cell">
    <style:table-cell-properties fo:background-color="{self.tint}" fo:padding-left="0.2cm" fo:padding-right="0.2cm" fo:padding-top="0.12cm" fo:padding-bottom="0.12cm" fo:border-left="none" fo:border-right="none" fo:border-top="none" fo:border-bottom="1pt solid {self.accent}"/>
  </style:style>
  <style:style style:name="CellBody" style:family="table-cell">
    <style:table-cell-properties fo:padding-left="0.2cm" fo:padding-right="0.2cm" fo:padding-top="0.12cm" fo:padding-bottom="0.12cm" fo:border-left="none" fo:border-right="none" fo:border-top="none" fo:border-bottom="0.5pt solid {RULE}"/>
  </style:style>
  <style:style style:name="CellKey" style:family="table-cell">
    <style:table-cell-properties fo:background-color="{self.tint}" fo:padding-left="0.2cm" fo:padding-right="0.2cm" fo:padding-top="0.12cm" fo:padding-bottom="0.12cm" fo:border-left="none" fo:border-right="none" fo:border-top="none" fo:border-bottom="0.5pt solid #ffffff"/>
  </style:style>
  """ + "\n  ".join(self.styles)

    def table(self, widths, rows, header=True, keys=False):
        """widths in cm; rows of cell texts. keys: the first column is a
        tinted label column (a details block) instead of a header row."""
        self.count += 1
        name = f"Table{self.count}"
        total = sum(widths)
        self.styles.append(f'<style:style style:name="{name}" style:family="table"><style:table-properties style:width="{total:.2f}cm" table:align="left" fo:margin-bottom="0.3cm"/></style:style>')
        cols = []
        for i, w in enumerate(widths):
            cname = f"{name}.C{i}"
            self.styles.append(f'<style:style style:name="{cname}" style:family="table-column"><style:table-column-properties style:column-width="{w:.2f}cm"/></style:style>')
            cols.append(f'<table:table-column table:style-name="{cname}"/>')
        body = []
        for r, row in enumerate(rows):
            cells = []
            for c, text in enumerate(row):
                if header and r == 0:
                    cell, para = "CellHead", "Table_20_Heading"
                elif keys and c == 0:
                    cell, para = "CellKey", "Table_20_Heading"
                else:
                    cell, para = "CellBody", "Table_20_Contents"
                cells.append(f'<table:table-cell table:style-name="{cell}" office:value-type="string"><text:p text:style-name="{para}">{inline(text)}</text:p></table:table-cell>')
            if header and r == 0:
                body.append(f'<table:table-header-rows><table:table-row>{"".join(cells)}</table:table-row></table:table-header-rows>')
            else:
                body.append(f'<table:table-row>{"".join(cells)}</table:table-row>')
        return f'<table:table table:name="{name}" table:style-name="{name}">{"".join(cols)}{"".join(body)}</table:table>'


def fodt(title, accent, body, automatic="", margins="2.5cm", footer=None):
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<office:document {NS} office:version="1.3" office:mimetype="application/vnd.oasis.opendocument.text">
{meta(title)}
{FONTS}
{writer_styles(accent)}
<office:automatic-styles>
  {page_layout(margins, footer)}
  {automatic}
</office:automatic-styles>
{master(footer)}
<office:body><office:text>
{body}
</office:text></office:body>
</office:document>
"""


TODAY = '<text:date style:data-style-name="NDateLong" text:fixed="false"/>'
TEXT_WIDTH = 16.0


def letter():
    right = '<style:style style:name="PRight" style:family="paragraph" style:parent-style-name="Text_20_body"><style:paragraph-properties fo:text-align="end" fo:margin-bottom="0.6cm"/></style:style>'
    gap = '<style:style style:name="PGap" style:family="paragraph" style:parent-style-name="Text_20_body"><style:paragraph-properties fo:margin-bottom="0.6cm"/></style:style>'
    sign = '<style:style style:name="PSign" style:family="paragraph" style:parent-style-name="Text_20_body"><style:paragraph-properties fo:margin-top="1.4cm"/></style:style>'
    body = "".join([
        p("Sam Taylor\n14 Harbour View\nFalmouth\nTR11 3AB\n07700 900123\nsam.taylor@example.co.uk", "PRight"),
        f'<text:p text:style-name="PGap">{TODAY}</text:p>',
        p("Ms Jo Patel\nCustomer Services\nCoastline Energy\n2 Quay Street\nBristol\nBS1 4DJ", "PGap"),
        p("**Account number 40817263: change of address**"),
        p("Dear Ms Patel,"),
        p("I am writing to let you know that I moved to the address above on 1 November 2026, and to ask you to move my account with me."),
        p("My final meter readings at my old address, 3 Mill Lane, Penryn, TR10 8AA, were 18452 for electricity and 07391 for gas, both taken on the morning I moved out. Please send the final bill for that address here."),
        p("I would like to keep paying by direct debit on the same date each month. If you need anything else from me to set up the new address, you can reach me on the number above or by email."),
        p("Thank you for your help."),
        p("Yours sincerely,"),
        p("Sam Taylor", "PSign"),
    ])
    return fodt("Letter", BLUE, body, right + gap + sign)


def cv():
    t = WriterTables(BLUE, TINT_BLUE)
    tabs = f'<style:tab-stops><style:tab-stop style:position="{TEXT_WIDTH}cm" style:type="right"/></style:tab-stops>'
    role = f'<style:style style:name="PRole" style:family="paragraph" style:parent-style-name="Text_20_body"><style:paragraph-properties fo:margin-top="0.2cm" fo:margin-bottom="0.05cm" fo:keep-with-next="always">{tabs}</style:paragraph-properties></style:style>'
    org = '<style:style style:name="POrg" style:family="paragraph" style:parent-style-name="Text_20_body"><style:paragraph-properties fo:margin-bottom="0.12cm" fo:keep-with-next="always"/><style:text-properties fo:color="' + MUTED + '"/></style:style>'
    contact = '<style:style style:name="PContact" style:family="paragraph" style:parent-style-name="Text_20_body"><style:paragraph-properties fo:margin-bottom="0.2cm" fo:padding-bottom="0.3cm" fo:border-bottom="0.75pt solid ' + RULE + '" fo:border-top="none" fo:border-left="none" fo:border-right="none"/><style:text-properties fo:color="' + MUTED + '"/></style:style>'
    dates = '<style:style style:name="TDates" style:family="text"><style:text-properties fo:color="' + MUTED + '" fo:font-weight="normal"/></style:style>'

    def job(title, place, when, points):
        return (f'<text:p text:style-name="PRole"><text:span text:style-name="Strong_20_Emphasis">{escape(title)}</text:span>'
                f'<text:tab/><text:span text:style-name="TDates">{escape(when)}</text:span></text:p>'
                + p(place, "POrg") + bullets(points))

    body = "".join([
        p("Jordan Ellis", "Title"),
        p("Manchester   ·   07700 900456   ·   jordan.ellis@example.co.uk   ·   linkedin.com/in/jordanellis", "PContact"),
        h(1, "Profile"),
        p("Organised operations coordinator with four years in logistics and three in customer service. I keep deliveries on time, spot problems before customers do, and like making a process simpler for the people who use it every day. Looking for a planning role with more responsibility for routes and suppliers."),
        h(1, "Experience"),
        job("Operations Coordinator", "Northside Logistics, Trafford Park", "March 2022 to now", [
            "Plan daily routes for a fleet of 18 vans, with on-time delivery up from 91% to 97% in my first year.",
            "Look after bookings with six regional suppliers and agree delivery windows that suit their sites.",
            "Moved the depot's paper job sheets onto a shared spreadsheet, saving the team about five hours a week.",
        ]),
        job("Customer Service Adviser", "Brightway Retail, Salford", "June 2019 to February 2022", [
            "Answered around 60 calls and emails a day about orders, returns and deliveries.",
            "Trained eight new starters and wrote the team's guide to the returns process.",
            "Named adviser of the quarter twice for customer feedback scores.",
        ]),
        h(1, "Education"),
        f'<text:p text:style-name="PRole"><text:span text:style-name="Strong_20_Emphasis">BA (Hons) Business Management, 2:1</text:span><text:tab/><text:span text:style-name="TDates">2016 to 2019</text:span></text:p>',
        p("University of Salford", "POrg"),
        f'<text:p text:style-name="PRole"><text:span text:style-name="Strong_20_Emphasis">A levels</text:span><text:tab/><text:span text:style-name="TDates">2014 to 2016</text:span></text:p>',
        p("Business (A), English Literature (B), Geography (B). Eccles Sixth Form College", "POrg"),
        h(1, "Skills"),
        t.table([4.0, 12.0], [
            ["Planning", "Route planning, stock and delivery scheduling, supplier bookings"],
            ["Software", "Excel and Google Sheets, Microsoft 365, SAP, Descartes route planning"],
            ["Licences", "Full UK driving licence, forklift (counterbalance)"],
            ["Languages", "English (native), Spanish (conversational)"],
        ], header=False, keys=True),
        h(1, "References"),
        p("Available on request."),
    ])
    return fodt("CV", BLUE, body, t.automatic() + role + org + contact + dates, margins="2cm")


def report():
    t = WriterTables(BLUE, TINT_BLUE)
    # The band across the top of the cover, in the colours of the card on
    # the start screen: a shape, so it survives a save as .docx.
    band = f"""<style:style style:name="gr1" style:family="graphic">
    <style:graphic-properties draw:stroke="none" draw:fill="gradient" draw:fill-gradient-name="Band" style:run-through="background" style:wrap="none" style:vertical-pos="top" style:vertical-rel="page" style:horizontal-pos="center" style:horizontal-rel="page"/>
  </style:style>"""
    gradient = f'<draw:gradient draw:name="Band" draw:style="linear" draw:start-color="#7f9cff" draw:end-color="#f0a3ae" draw:start-intensity="100%" draw:end-intensity="100%" draw:angle="135deg" draw:border="0%"/>'
    first = '<style:style style:name="PFirst" style:family="paragraph" style:parent-style-name="Label"><style:paragraph-properties fo:margin-top="3.6cm"/></style:style>'
    byline = '<style:style style:name="PBy" style:family="paragraph" style:parent-style-name="Text_20_body"><style:paragraph-properties fo:margin-bottom="0.6cm" fo:padding-bottom="0.4cm" fo:border-bottom="0.75pt solid ' + RULE + '" fo:border-top="none" fo:border-left="none" fo:border-right="none"/><style:text-properties fo:color="' + MUTED + '"/></style:style>'
    shape = ('<draw:rect text:anchor-type="paragraph" draw:z-index="0" draw:style-name="gr1" '
             'svg:width="21cm" svg:height="4.6cm" svg:x="0cm" svg:y="0cm"><text:p/></draw:rect>')
    body = "".join([
        f'<text:p text:style-name="PFirst">{shape}Progress report</text:p>',
        p("Community garden project", "Title"),
        p("July to September 2026", "Subtitle"),
        p("Prepared by Priya Shah for the Eastfield Residents’ Association, 6 October 2026", "PBy"),
        h(1, "Summary"),
        p("The garden opened to the public on 12 July. In its first three months 41 households signed up for plots, volunteers gave just over 600 hours, and we spent £3,240 of the £5,000 grant. The main problem has been water: the single tap is not enough for a dry summer, and fixing that is our first recommendation."),
        h(1, "Background"),
        p("The council leased us the unused ground behind Eastfield Library in March 2026 for five years. The aim is to give people without gardens somewhere to grow food, and to bring neighbours together. The project is run by volunteers and paid for by a community grant and plot fees."),
        h(1, "What we found"),
        h(2, "Plots and members"),
        p("Demand was higher than we planned for. All 30 full plots and 22 half plots are taken, and there are 14 households on the waiting list."),
        t.table([7.0, 3.0, 3.0, 3.0], [
            ["Measure", "Target", "Actual", "Status"],
            ["Households with a plot", "35", "41", "Ahead"],
            ["Volunteer hours", "450", "612", "Ahead"],
            ["Open days held", "4", "3", "Behind"],
            ["Grant spent", "£3,000", "£3,240", "On track"],
        ]),
        h(2, "Water and tools"),
        p("On hot days members queued for the tap at the end of the evening, and two plots were given up in August because of it. The tool shed works well, but the wheelbarrows and hoses need replacing sooner than we expected."),
        h(2, "What people told us"),
        p("We asked members what they thought in September and 29 replied. Most said the garden had helped them meet neighbours, and the most common request was for a covered seating area."),
        p("“I’ve lived on this street for eleven years and this summer is the first time I’ve known the names of the people next door.”", "Quotations"),
        h(1, "Recommendations"),
        bullets([
            "Fit two water butts on the shed roof and ask the council for a second tap before next spring.",
            "Offer the 14 waiting households half plots by splitting the four largest plots.",
            "Use the remaining grant for a covered seating area by the gate.",
            "Hold the missed open day in October, with a harvest sale to raise money for tools.",
        ], numbered=True),
        h(1, "Next steps"),
        p("The committee will discuss these recommendations at its meeting on 20 October. If they are agreed, Priya will contact the council about the second tap and get quotes for the seating area."),
    ])
    return fodt("Report", BLUE, body, band + t.automatic() + first + byline, footer="Community garden project, progress report").replace(
        "<office:styles>", "<office:styles>\n  " + gradient, 1)


def newsletter():
    columns = """<style:style style:name="Sect1" style:family="section">
    <style:section-properties text:dont-balance-text-columns="false" style:editable="false">
      <style:columns fo:column-count="2" fo:column-gap="0.8cm"><style:column-sep style:width="0.5pt" style:color="#d7dbe3" style:height="100%" style:style="solid" style:vertical-align="top"/><style:column style:rel-width="32767*" fo:start-indent="0cm" fo:end-indent="0.4cm"/><style:column style:rel-width="32767*" fo:start-indent="0.4cm" fo:end-indent="0cm"/></style:columns>
    </style:section-properties>
  </style:style>"""
    lead = f'<style:style style:name="PLead" style:family="paragraph" style:parent-style-name="Text_20_body"><style:paragraph-properties fo:margin-bottom="0.5cm" fo:padding-bottom="0.45cm" fo:border-bottom="1.5pt solid {GREEN}" fo:border-top="none" fo:border-left="none" fo:border-right="none"/><style:text-properties fo:font-size="12.5pt"/></style:style>'
    first = '<style:style style:name="PFirstHead" style:family="paragraph" style:parent-style-name="Heading_20_1"><style:paragraph-properties fo:margin-top="0cm"/></style:style>'
    body = "".join([
        p("Harbour Rowing Club", "Label"),
        p("Spring newsletter", "Title"),
        p("Issue 12, April 2027", "Subtitle"),
        p("Welcome back after a long, wet winter. The new launch arrived in February and the boat house finally has heating that works, so early sessions are a lot more bearable than they were.", "PLead"),
        '<text:section text:style-name="Sect1" text:name="Columns">',
        f'<text:h text:style-name="PFirstHead" text:outline-level="1">Training times</text:h>',
        p("From April, Tuesday sessions go back to 18:30 and Thursday stays at 18:00. Saturday mornings start at 08:00 on the high tide, so check the board before you set off."),
        p("Juniors train on Saturdays from 10:00. Parents, please stay until your child is back on the pontoon."),
        h(1, "Summer regatta"),
        p("We’re hosting again this year on Saturday 18 July. We need volunteers for:"),
        bullets(["The start line and timing", "The tea tent", "Safety boats (you’ll need your powerboat level 2)"]),
        p("Please put your name on the list in the club room by the end of May."),
        h(1, "New members"),
        p("A warm welcome to the eleven people who finished the learn to row course in March. If you see someone you don’t recognise at the boat house, say hello."),
        h(1, "Boat house jobs"),
        p("The spring work party is on Sunday 26 April from 09:00. Jobs include washing the fleet, repainting the oar racks and clearing the slipway. Bacon rolls at 11:00 for everyone who helps."),
        h(1, "Dates for the diary"),
        bullets([
            "**26 April**, spring work party",
            "**9 May**, club head race",
            "**13 June**, summer social at the Ship Inn",
            "**18 July**, Harbour regatta",
        ]),
        '</text:section>',
        p(""),
    ])
    return fodt("Newsletter", GREEN, body, columns + lead + first, margins="2cm",
                footer="Harbour Rowing Club   ·   The Boat House, Quay Road   ·   hello@harbourrowing.example")


def meeting_notes():
    t = WriterTables(BLUE, TINT_BLUE)
    body = "".join([
        p("Eastfield Residents’ Association", "Label"),
        p("Committee meeting", "Title"),
        t.table([3.6, 12.4], [
            ["Date", "TODAY"],
            ["Time", "19:00 to 20:30"],
            ["Where", "Eastfield Library, community room"],
            ["Chair", "Priya Shah"],
            ["Present", "Priya Shah, Tom Okafor, Helen Burns, Marek Nowak, Grace Lee"],
            ["Apologies", "David Price"],
        ], header=False, keys=True).replace(
            '<text:p text:style-name="Table_20_Contents">TODAY</text:p>',
            f'<text:p text:style-name="Table_20_Contents">{TODAY}</text:p>'),
        h(1, "Agenda"),
        bullets([
            "Notes from the last meeting",
            "Community garden progress report",
            "Summer street party",
            "Any other business",
        ], numbered=True),
        h(1, "Notes"),
        h(2, "1. Notes from the last meeting"),
        p("Agreed as a correct record. The litter pick on 14 September went ahead with 22 people."),
        h(2, "2. Community garden progress report"),
        p("Priya went through the report. The committee agreed all four recommendations, and asked for quotes for the seating area before the next meeting."),
        h(2, "3. Summer street party"),
        p("Helen has spoken to the council about closing Cedar Road for the day. The application has to be in eight weeks before. Saturday 4 July was the most popular date."),
        h(2, "4. Any other business"),
        p("Marek raised the broken street light outside number 40. Tom will report it again."),
        h(1, "Actions"),
        t.table([9.0, 3.5, 3.5], [
            ["Action", "Who", "By when"],
            ["Ask the council for a second tap at the garden", "Priya", "31 October"],
            ["Get three quotes for the seating area", "Grace", "17 November"],
            ["Send the road closure application", "Helen", "1 May"],
            ["Report the street light outside number 40", "Tom", "This week"],
        ]),
        h(1, "Next meeting"),
        p("Tuesday 17 November, 19:00, Eastfield Library."),
    ])
    return fodt("Meeting notes", BLUE, body, t.automatic())


def writer_default():
    return fodt("Document", BLUE, '<text:p text:style-name="Standard"/>')


# Calc

def col_letter(n):
    s = ""
    n += 1
    while n:
        n, r = divmod(n - 1, 26)
        s = chr(65 + r) + s
    return s


CALC_NUMBERS = """
  <number:currency-style style:name="NGbpPos" style:volatile="true">
    <number:currency-symbol number:language="en" number:country="GB">£</number:currency-symbol>
    <number:number number:decimal-places="2" number:min-decimal-places="2" number:min-integer-digits="1" number:grouping="true"/>
  </number:currency-style>
  <number:currency-style style:name="NGbp">
    <style:text-properties fo:color="#c5372c"/>
    <number:text>-</number:text>
    <number:currency-symbol number:language="en" number:country="GB">£</number:currency-symbol>
    <number:number number:decimal-places="2" number:min-decimal-places="2" number:min-integer-digits="1" number:grouping="true"/>
    <style:map style:condition="value()&gt;=0" style:apply-style-name="NGbpPos"/>
  </number:currency-style>
  <number:date-style style:name="NDate">
    <number:day number:style="long"/><number:text>/</number:text><number:month number:style="long"/><number:text>/</number:text><number:year number:style="long"/>
  </number:date-style>
  <number:date-style style:name="NDay">
    <number:day-of-week number:style="long"/><number:text> </number:text><number:day/><number:text> </number:text><number:month number:textual="true"/>
  </number:date-style>
  <number:date-style style:name="NDayShort">
    <number:day/><number:text> </number:text><number:month number:textual="true"/>
  </number:date-style>
  <number:time-style style:name="NTime">
    <number:hours number:style="long"/><number:text>:</number:text><number:minutes number:style="long"/>
  </number:time-style>
  <number:number-style style:name="NHours">
    <number:number number:decimal-places="2" number:min-decimal-places="2" number:min-integer-digits="1"/>
  </number:number-style>
  <number:number-style style:name="NWhole">
    <number:number number:decimal-places="0" number:min-decimal-places="0" number:min-integer-digits="1" number:grouping="true"/>
  </number:number-style>
  <number:percentage-style style:name="NPercent">
    <number:number number:decimal-places="0" number:min-decimal-places="0" number:min-integer-digits="1"/><number:text>%</number:text>
  </number:percentage-style>
"""


def calc_styles():
    return f"""<office:styles>
  {CALC_NUMBERS}
  <style:default-style style:family="table-cell">
    <style:paragraph-properties style:tab-stop-distance="1.25cm"/>
    <style:text-properties style:font-name="Carlito" fo:font-size="11pt" {LANG}/>
  </style:default-style>
  <style:style style:name="Default" style:family="table-cell">
    <style:table-cell-properties style:vertical-align="middle"/>
    <style:text-properties fo:color="{INK}"/>
  </style:style>
  <style:style style:name="Heading" style:family="table-cell" style:parent-style-name="Default">
    <style:text-properties fo:font-size="18pt" fo:font-weight="bold"/>
  </style:style>
  <style:style style:name="Heading_20_1" style:display-name="Heading 1" style:family="table-cell" style:parent-style-name="Heading">
    <style:text-properties fo:font-size="20pt"/>
  </style:style>
  <style:style style:name="Heading_20_2" style:display-name="Heading 2" style:family="table-cell" style:parent-style-name="Heading">
    <style:text-properties fo:font-size="13pt" fo:color="{BLUE}"/>
  </style:style>
  <style:style style:name="Note" style:family="table-cell" style:parent-style-name="Default">
    <style:text-properties fo:color="{MUTED}"/>
  </style:style>
  <style:style style:name="Accent" style:family="table-cell" style:parent-style-name="Default">
    <style:text-properties fo:font-weight="bold"/>
  </style:style>
  <style:style style:name="Currency" style:family="table-cell" style:parent-style-name="Default" style:data-style-name="NGbp"/>
  <style:style style:name="Percent" style:family="table-cell" style:parent-style-name="Default" style:data-style-name="NPercent"/>
</office:styles>"""


class Sheet:
    """One sheet: column widths, row heights and cells by A1 address."""

    def __init__(self, name, widths, print_cols=None):
        self.name = name
        self.widths = widths
        self.cells = {}
        self.heights = {}
        self.merges = {}
        self.validations = {}
        self.print_cols = print_cols or len(widths)

    def set(self, ref, value=None, style="body", kind=None, formula=None, span=None):
        col = 0
        i = 0
        while ref[i].isalpha():
            col = col * 26 + ord(ref[i]) - 64
            i += 1
        row = int(ref[i:]) - 1
        self.cells[(row, col - 1)] = (value, style, kind, formula)
        if span:
            self.merges[(row, col - 1)] = span

    def height(self, row, cm):
        self.heights[row - 1] = cm


class Calc:
    """Cell styles shared by every Tables template, in the TablesDoc look:
    a pale header band in bold, hairlines between rows, a dark rule above
    totals, money in pounds with negatives in red."""

    BASE = {
        "title": ('fo:font-size="20pt" fo:font-weight="bold"', "", None),
        "subtitle": (f'fo:color="{MUTED}"', "", None),
        "section": (f'fo:font-size="13pt" fo:font-weight="bold" fo:color="{{accent}}"', "", None),
        "head": ('fo:font-weight="bold"', f'fo:background-color="{{tint}}" fo:border-bottom="1pt solid {{accent}}"', None),
        "headr": ('fo:font-weight="bold"', f'fo:background-color="{{tint}}" fo:border-bottom="1pt solid {{accent}}"', "end"),
        "body": ("", f'fo:border-bottom="0.5pt solid {RULE}"', None),
        "bodyc": ("", f'fo:border-bottom="0.5pt solid {RULE}"', "center"),
        "money": ("", f'fo:border-bottom="0.5pt solid {RULE}"', None, "NGbp"),
        "moneyb": ('fo:font-weight="bold"', f'fo:border-bottom="0.5pt solid {RULE}"', None, "NGbp"),
        "date": ("", f'fo:border-bottom="0.5pt solid {RULE}"', None, "NDate"),
        "day": ("", f'fo:border-bottom="0.5pt solid {RULE}"', None, "NDay"),
        "time": ("", f'fo:border-bottom="0.5pt solid {RULE}"', "center", "NTime"),
        "hours": ("", f'fo:border-bottom="0.5pt solid {RULE}"', None, "NHours"),
        "whole": ("", f'fo:border-bottom="0.5pt solid {RULE}"', None, "NWhole"),
        "total": ('fo:font-weight="bold"', f'fo:border-top="1pt solid {INK}"', None),
        "totalmoney": ('fo:font-weight="bold"', f'fo:border-top="1pt solid {INK}"', None, "NGbp"),
        "totalhours": ('fo:font-weight="bold"', f'fo:border-top="1pt solid {INK}"', None, "NHours"),
        "grand": ('fo:font-weight="bold" fo:font-size="13pt"', 'fo:background-color="{tint}"', None),
        "grandmoney": ('fo:font-weight="bold" fo:font-size="13pt"', 'fo:background-color="{tint}"', None, "NGbp"),
        "key": (f'fo:color="{MUTED}"', "", None),
        "keyr": (f'fo:color="{MUTED}"', "", "end"),
        "plain": ("", "", None),
        "plainb": ('fo:font-weight="bold"', "", None),
        "plainr": ("", "", "end"),
        "plaindate": ("", "", "end", "NDate"),
        "plainmoney": ("", "", None, "NGbp"),
        "percent": ("", f'fo:border-bottom="0.5pt solid {RULE}"', None, "NPercent"),
        "wrap": ("", f'fo:border-bottom="0.5pt solid {RULE}"', None, None, True),
        "slot": (f'fo:color="{MUTED}" fo:font-size="10pt"', f'fo:border-bottom="0.5pt solid {RULE}" fo:border-right="0.5pt solid {RULE}"', "end"),
        "cell": ("", f'fo:border-bottom="0.5pt solid {RULE}" fo:border-right="0.5pt solid {RULE}"', None, None, True),
        "dayhead": ('fo:font-weight="bold"', f'fo:background-color="{{tint}}" fo:border-bottom="1pt solid {{accent}}" fo:border-right="0.5pt solid #ffffff"', "center"),
        "daydate": (f'fo:color="{MUTED}"', f'fo:background-color="{{tint}}" fo:border-bottom="1pt solid {{accent}}" fo:border-right="0.5pt solid #ffffff"', "center", "NDayShort"),
        "box": ("", f'fo:border="0.5pt solid {RULE}"', None, None, True),
    }

    def __init__(self, accent=BLUE, tint=TINT_BLUE):
        self.accent = accent
        self.tint = tint

    def styles(self):
        out = []
        for name, spec in self.BASE.items():
            text, cell, align = spec[0], spec[1], spec[2]
            data = spec[3] if len(spec) > 3 else None
            wrap = spec[4] if len(spec) > 4 else False
            text = text.replace("{accent}", self.accent)
            cell = cell.replace("{accent}", self.accent).replace("{tint}", self.tint)
            ds = f' style:data-style-name="{data}"' if data else ""
            props = f'{cell} style:vertical-align="{"top" if wrap else "middle"}"'
            if wrap:
                props += ' fo:wrap-option="wrap"'
            if cell or data:
                props += ' fo:padding-left="0.1cm" fo:padding-right="0.1cm"'
            para = f'<style:paragraph-properties fo:text-align="{align}"/>' if align else ""
            if align:
                props += ' style:text-align-source="fix"'
            out.append(f'<style:style style:name="c-{name}" style:family="table-cell" style:parent-style-name="Default"{ds}>'
                       f'<style:table-cell-properties {props}/>{para}<style:text-properties {text}/></style:style>')
        return "\n  ".join(out)


def cell_xml(value, style, kind, formula, span):
    attrs = [f'table:style-name="c-{style}"']
    if span:
        attrs.append(f'table:number-columns-spanned="{span}" table:number-rows-spanned="1"')
    if formula:
        attrs.append(f'table:formula="{escape(formula, {chr(34): "&quot;"})}"')
    text = ""
    if kind == "money":
        attrs.append(f'office:value-type="currency" office:currency="GBP" office:value="{value or 0}"')
    elif kind == "number":
        attrs.append(f'office:value-type="float" office:value="{value or 0}"')
    elif kind == "percent":
        attrs.append(f'office:value-type="percentage" office:value="{value}"')
    elif kind == "date":
        attrs.append(f'office:value-type="date" office:date-value="{value}"')
    elif kind == "time":
        hh, mm = value.split(":")
        attrs.append(f'office:value-type="time" office:time-value="PT{hh}H{mm}M00S"')
    elif value is not None and value != "":
        attrs.append('office:value-type="string"')
        text = "".join(f"<text:p>{escape(line)}</text:p>" for line in str(value).split("\n"))
    elif formula:
        attrs.append('office:value-type="float" office:value="0"')
    return f'<table:table-cell {" ".join(attrs)}>{text}</table:table-cell>'


def fods(title, sheets, calc, grid=True, landscape=False, fit_pages=None, validations=""):
    auto = [calc.styles()]
    tables = []
    for si, sh in enumerate(sheets):
        cols = []
        for i, w in enumerate(sh.widths):
            name = f"co{si}_{i}"
            auto.append(f'<style:style style:name="{name}" style:family="table-column"><style:table-column-properties fo:break-before="auto" style:column-width="{w}cm"/></style:style>')
            cols.append(f'<table:table-column table:style-name="{name}" table:default-cell-style-name="Default"/>')
        rows = []
        last = max(r for r, _ in sh.cells) if sh.cells else 0
        for r in range(last + 1):
            hname = f"ro{si}_{r}"
            hgt = sh.heights.get(r, 0.55)
            auto.append(f'<style:style style:name="{hname}" style:family="table-row"><style:table-row-properties style:row-height="{hgt}cm" fo:break-before="auto" style:use-optimal-row-height="false"/></style:style>')
            cells = []
            c = 0
            covered = 0
            while c < len(sh.widths):
                if covered:
                    cells.append('<table:covered-table-cell/>')
                    covered -= 1
                    c += 1
                    continue
                if (r, c) in sh.cells:
                    value, style, kind, formula = sh.cells[(r, c)]
                    span = sh.merges.get((r, c))
                    cells.append(cell_xml(value, style, kind, formula, span))
                    if span:
                        covered = span - 1
                else:
                    cells.append("<table:table-cell/>")
                c += 1
            rows.append(f'<table:table-row table:style-name="{hname}">{"".join(cells)}</table:table-row>')
        area = f"$'{sh.name}'.$A$1:.${col_letter(sh.print_cols - 1)}${last + 1}"
        tables.append(f'<table:table table:name="{escape(sh.name)}" table:style-name="ta1" table:print-ranges="{escape(area)}">'
                      f'{"".join(cols)}{"".join(rows)}</table:table>')
    width, height = ("29.7cm", "21cm") if landscape else ("21cm", "29.7cm")
    scale = f'style:scale-to-pages="{fit_pages}"' if fit_pages else 'loext:scale-to-X="1" loext:scale-to-Y="0"'
    auto.append(f"""<style:style style:name="ta1" style:family="table" style:master-page-name="Default"><style:table-properties table:display="true" style:writing-mode="lr-tb"/></style:style>
  <style:page-layout style:name="pm1">
    <style:page-layout-properties fo:page-width="{width}" fo:page-height="{height}" style:print-orientation="{"landscape" if landscape else "portrait"}" fo:margin-top="1.5cm" fo:margin-bottom="1.5cm" fo:margin-left="1.5cm" fo:margin-right="1.5cm" style:print="objects charts drawings zero-values" style:print-page-order="ttb" style:first-page-number="continue" {scale} style:writing-mode="lr-tb"/>
    <style:header-style/><style:footer-style/>
  </style:page-layout>""")
    settings = f"""<office:settings>
  <config:config-item-set config:name="ooo:view-settings">
    <config:config-item-map-indexed config:name="Views"><config:config-item-map-entry>
      <config:config-item config:name="ViewId" config:type="string">view1</config:config-item>
      <config:config-item config:name="ActiveTable" config:type="string">{escape(sheets[0].name)}</config:config-item>
      <config:config-item config:name="ShowGrid" config:type="boolean">{"true" if grid else "false"}</config:config-item>
      <config:config-item config:name="ZoomValue" config:type="int">100</config:config-item>
      <config:config-item config:name="ShowZeroValues" config:type="boolean">true</config:config-item>
      <config:config-item config:name="HasColumnRowHeaders" config:type="boolean">true</config:config-item>
      <config:config-item config:name="HasSheetTabs" config:type="boolean">true</config:config-item>
    </config:config-item-map-entry></config:config-item-map-indexed>
  </config:config-item-set>
</office:settings>"""
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<office:document {NS} office:version="1.3" office:mimetype="application/vnd.oasis.opendocument.spreadsheet">
{meta(title)}
{settings}
{FONTS}
{calc_styles()}
<office:automatic-styles>
  {chr(10).join(auto)}
</office:automatic-styles>
<office:master-styles>
  <style:master-page style:name="Default" style:page-layout-name="pm1">
    <style:header style:display="false"/><style:footer style:display="false"/>
  </style:master-page>
</office:master-styles>
<office:body><office:spreadsheet>
{validations}
{"".join(tables)}
</office:spreadsheet></office:body>
</office:document>
"""


def budget():
    calc = Calc()
    s = Sheet("Budget", [6.2, 3.2, 3.2, 3.2, 0.5])
    s.set("A1", "Monthly budget", "title")
    s.height(1, 1.0)
    s.set("A2", "Put in what you plan to spend and what you really spent. The totals and differences work themselves out.", "subtitle", span=4)
    s.height(2, 0.7)
    row = 4
    s.set(f"A{row}", "Income", "section")
    s.height(row, 0.8)
    row += 1
    for col, text in zip("ABCD", ["", "Planned", "Actual", "Difference"]):
        s.set(f"{col}{row}", text, "headr" if col != "A" else "head")
    income = [("Pay after tax", 2450, 2450), ("Other income", 150, 210)]
    first_in = row + 1
    for name, plan, actual in income:
        row += 1
        s.set(f"A{row}", name)
        s.set(f"B{row}", plan, "money", "money")
        s.set(f"C{row}", actual, "money", "money")
        s.set(f"D{row}", None, "money", "money", f"of:=[.C{row}]-[.B{row}]")
    row += 1
    total_in = row
    s.set(f"A{row}", "Total income", "total")
    for col in "BCD":
        s.set(f"{col}{row}", None, "totalmoney", "money", f"of:=SUM([.{col}{first_in}:.{col}{row - 1}])")
    row += 2
    s.set(f"A{row}", "Spending", "section")
    s.height(row, 0.8)
    row += 1
    for col, text in zip("ABCD", ["", "Planned", "Actual", "Left"]):
        s.set(f"{col}{row}", text, "headr" if col != "A" else "head")
    spending = [
        ("Rent or mortgage", 850, 850), ("Council tax", 142, 142), ("Gas and electricity", 120, 134),
        ("Water", 38, 38), ("Phone and broadband", 55, 55), ("Food shopping", 320, 347),
        ("Travel", 140, 118), ("Insurance", 45, 45), ("Subscriptions", 30, 30),
        ("Eating out", 80, 96), ("Clothes", 50, 22), ("Presents", 25, 40), ("Savings", 300, 300),
    ]
    first_out = row + 1
    for name, plan, actual in spending:
        row += 1
        s.set(f"A{row}", name)
        s.set(f"B{row}", plan, "money", "money")
        s.set(f"C{row}", actual, "money", "money")
        s.set(f"D{row}", None, "money", "money", f"of:=[.B{row}]-[.C{row}]")
    row += 1
    total_out = row
    s.set(f"A{row}", "Total spending", "total")
    for col in "BCD":
        s.set(f"{col}{row}", None, "totalmoney", "money", f"of:=SUM([.{col}{first_out}:.{col}{row - 1}])")
    row += 2
    s.set(f"A{row}", "Left over this month", "grand")
    s.set(f"B{row}", None, "grandmoney", "money", f"of:=[.B{total_in}]-[.B{total_out}]")
    s.set(f"C{row}", None, "grandmoney", "money", f"of:=[.C{total_in}]-[.C{total_out}]")
    s.set(f"D{row}", "", "grand")
    s.height(row, 0.8)
    return fods("Budget", [s], calc)


def invoice():
    calc = Calc()
    s = Sheet("Invoice", [8.4, 2.2, 3.1, 3.3])
    s.set("A1", "Invoice", "title")
    s.height(1, 1.1)
    for i, line in enumerate(["Riverside Joinery", "Unit 4, Mill Yard", "Hebden Bridge", "HX7 8AD", "hello@riversidejoinery.example", "01422 000 000"]):
        s.set(f"A{3 + i}", line, "plainb" if i == 0 else "plain")
    s.set("C3", "Invoice number", "keyr")
    s.set("D3", "INV-0001", "plainr")
    s.set("C4", "Date", "keyr")
    s.set("D4", "2026-11-10", "plaindate", "date")
    s.set("C5", "Due date", "keyr")
    s.set("D5", None, "plaindate", "date", "of:=[.D4]+30")
    s.set("C6", "Your reference", "keyr")
    s.set("D6", "PO 1187", "plainr")
    s.set("A10", "Bill to", "section")
    for i, line in enumerate(["Hannah Clarke", "The Old Bakery", "12 Market Street", "Todmorden", "OL14 5AJ"]):
        s.set(f"A{11 + i}", line, "plainb" if i == 0 else "plain")
    head = 17
    for col, text in zip("ABCD", ["Description", "Quantity", "Unit price", "Amount"]):
        s.set(f"{col}{head}", text, "head" if col == "A" else "headr")
    s.height(head, 0.7)
    items = [
        ("Oak kitchen shelves, made to measure", 3, 85),
        ("Fitting shelves and brackets", 1, 120),
        ("Pine skirting board, per metre", 14, 6.5),
        ("Labour, hours", 6, 35),
    ]
    for i in range(10):
        r = head + 1 + i
        name, qty, price = items[i] if i < len(items) else ("", None, None)
        s.set(f"A{r}", name)
        s.set(f"B{r}", qty, "whole", "number" if qty is not None else None)
        s.set(f"C{r}", price, "money", "money" if price is not None else None)
        s.set(f"D{r}", None, "money", None, f'of:=IF([.B{r}]="";"";[.B{r}]*[.C{r}])')
    end = head + 10
    sub = end + 2
    s.set(f"C{sub}", "Subtotal", "keyr")
    s.set(f"D{sub}", None, "plainmoney", "money", f"of:=SUM([.D{head + 1}:.D{end}])")
    s.set(f"B{sub + 1}", 0.2, "percent", "percent")
    s.set(f"C{sub + 1}", "VAT", "keyr")
    s.set(f"D{sub + 1}", None, "plainmoney", "money", f"of:=ROUND([.D{sub}]*[.B{sub + 1}];2)")
    s.set(f"C{sub + 2}", "Total due", "grand")
    s.set(f"D{sub + 2}", None, "grandmoney", "money", f"of:=[.D{sub}]+[.D{sub + 1}]")
    s.height(sub + 2, 0.8)
    pay = sub + 5
    s.set(f"A{pay}", "How to pay", "section")
    for i, (k, v) in enumerate([("Bank", "Northern Bank"), ("Account name", "Riverside Joinery"), ("Sort code", "00-00-00"), ("Account number", "12345678")]):
        s.set(f"A{pay + 1 + i}", f"{k}:  {v}", "plain")
    s.set(f"A{pay + 6}", "Please pay within 30 days, using the invoice number as the reference. Thank you.", "subtitle", span=4)
    return fods("Invoice", [s], calc, grid=False, fit_pages=1)


def planner():
    calc = Calc(GREEN, TINT_GREEN)
    days = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"]
    s = Sheet("Week", [1.6] + [3.4] * 7)
    s.set("A1", "Week planner", "title", span=4)
    s.height(1, 1.0)
    s.set("A2", "Week starting", "key", span=2)
    s.set("C2", "2026-11-09", "plaindate", "date")
    s.set("A4", "", "dayhead")
    for i, d in enumerate(days):
        col = col_letter(i + 1)
        s.set(f"{col}4", d, "dayhead")
        s.set(f"{col}5", None, "daydate", "date", f"of:=[.$C$2]+{i}")
    s.set("A5", "", "dayhead")
    s.height(4, 0.65)
    s.height(5, 0.6)
    plans = {
        ("07:00", 0): "Gym", ("07:00", 2): "Gym", ("09:00", 0): "Team catch-up",
        ("12:00", 2): "Lunch with Sarah", ("15:00", 3): "Dentist, 15:30", ("18:00", 1): "Swimming lessons",
        ("18:00", 3): "Swimming lessons", ("19:00", 4): "Film night", ("10:00", 5): "Parkrun, then market",
        ("13:00", 6): "Lunch at Mum’s", ("16:00", 4): "Dry cleaning",
    }
    for i, hour in enumerate(range(7, 22)):
        r = 6 + i
        slot = f"{hour:02d}:00"
        s.set(f"A{r}", slot, "slot")
        for d in range(7):
            s.set(f"{col_letter(d + 1)}{r}", plans.get((slot, d), ""), "cell")
        s.height(r, 0.85)
    r = 6 + 15 + 1
    s.set(f"A{r}", "This week", "section", span=4)
    s.set(f"E{r}", "Shopping", "section", span=4)
    s.height(r, 0.8)
    todo = ["Book the car in for its MOT", "Renew the house insurance", "Birthday card for Dad", "Return library books"]
    shopping = ["Milk, bread, eggs", "Washing up liquid", "Birthday present for Dad", "Batteries for the smoke alarm"]
    for i in range(5):
        s.set(f"A{r + 1 + i}", ("☐  " + todo[i]) if i < len(todo) else "☐", "body", span=4)
        s.set(f"E{r + 1 + i}", ("☐  " + shopping[i]) if i < len(shopping) else "☐", "body", span=4)
        s.height(r + 1 + i, 0.65)
    return fods("Planner", [s], calc, landscape=True, fit_pages=1)


def timesheet():
    calc = Calc()
    s = Sheet("Timesheet", [3.6, 2.6, 2.2, 2.2, 2.4, 2.2, 4.6])
    s.set("A1", "Timesheet", "title")
    s.height(1, 1.0)
    for i, (k, v, kind, style) in enumerate([
        ("Name", "Chris Morgan", None, "plainb"),
        ("Week starting", "2026-11-09", "date", "plaindate"),
        ("Pay per hour", 12.6, "money", "plainmoney"),
    ]):
        s.set(f"A{3 + i}", k, "key")
        s.set(f"B{3 + i}", v, style, kind, span=2)
    head = 7
    for col, text in zip("ABCDEFG", ["Day", "Date", "Start", "Finish", "Break (mins)", "Hours", "Notes"]):
        s.set(f"{col}{head}", text, "head" if col in "AG" else "headr")
    s.height(head, 0.7)
    days = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"]
    times = [("09:00", "17:30", 30, ""), ("09:00", "17:30", 30, ""), ("08:30", "16:00", 30, "Left early for the dentist"),
             ("09:00", "17:30", 30, ""), ("09:00", "18:30", 45, "Stocktake"), (None, None, None, ""), (None, None, None, "")]
    for i, d in enumerate(days):
        r = head + 1 + i
        start, finish, brk, note = times[i]
        s.set(f"A{r}", d)
        s.set(f"B{r}", None, "date", "date", f"of:=[.$B$4]+{i}")
        s.set(f"C{r}", start, "time", "time" if start else None)
        s.set(f"D{r}", finish, "time", "time" if finish else None)
        s.set(f"E{r}", brk, "whole", "number" if brk is not None else None)
        s.set(f"F{r}", None, "hours", None, f'of:=IF(OR([.C{r}]="";[.D{r}]="");"";ROUND(MOD([.D{r}]-[.C{r}];1)*24-[.E{r}]/60;2))')
        s.set(f"G{r}", note, "body")
        s.height(r, 0.65)
    end = head + 7
    s.set(f"A{end + 1}", "Total hours", "total")
    for col in "BCDEG":
        s.set(f"{col}{end + 1}", "", "total")
    s.set(f"F{end + 1}", None, "totalhours", None, f"of:=SUM([.F{head + 1}:.F{end}])")
    s.set(f"A{end + 3}", "Pay this week", "grand")
    s.set(f"B{end + 3}", None, "grandmoney", "money", f"of:=ROUND([.F{end + 1}]*[.B5];2)", span=2)
    s.height(end + 3, 0.8)
    s.set(f"A{end + 6}", "Signed", "key")
    s.set(f"B{end + 6}", "", "body", span=3)
    s.set(f"E{end + 6}", "Date", "keyr")
    s.set(f"F{end + 6}", "", "body", span=2)
    s.height(end + 6, 0.9)
    return fods("Timesheet", [s], calc, fit_pages=1)


def expenses():
    calc = Calc(GREEN, TINT_GREEN)
    s = Sheet("Expenses", [2.3, 5.2, 2.6, 2.6, 2.4, 0.4, 2.4, 2.3])
    s.set("A1", "Expenses", "title")
    s.height(1, 1.0)
    s.set("A2", "One line for each thing you pay for. Pick a category and the totals on the right add themselves up.", "subtitle", span=5)
    head = 4
    for col, text in zip("ABCDE", ["Date", "What for", "Category", "Paid with", "Amount"]):
        s.set(f"{col}{head}", text, "headr" if col == "E" else "head")
    s.height(head, 0.7)
    rows = [
        ("2026-11-01", "Weekly shop, Co-op", "Food", "Debit card", 64.20),
        ("2026-11-02", "Train to Leeds and back", "Travel", "Debit card", 18.40),
        ("2026-11-03", "Gas and electricity", "Bills", "Direct debit", 134.00),
        ("2026-11-04", "Lunch with Ali", "Eating out", "Credit card", 23.50),
        ("2026-11-05", "Phone contract", "Bills", "Direct debit", 22.00),
        ("2026-11-07", "Weekly shop, Aldi", "Food", "Debit card", 58.75),
        ("2026-11-08", "Cinema tickets", "Going out", "Credit card", 19.80),
        ("2026-11-08", "Winter coat", "Clothes", "Credit card", 89.00),
        ("2026-11-09", "Bus pass top-up", "Travel", "Debit card", 25.00),
    ]
    count = 40
    for i in range(count):
        r = head + 1 + i
        if i < len(rows):
            date, what, cat, how, amount = rows[i]
            s.set(f"A{r}", date, "date", "date")
            s.set(f"B{r}", what)
            s.set(f"C{r}", cat)
            s.set(f"D{r}", how)
            s.set(f"E{r}", amount, "money", "money")
        else:
            for col, style in zip("ABCDE", ["date", "body", "body", "body", "money"]):
                s.set(f"{col}{r}", None, style)
    end = head + count
    s.set(f"A{end + 1}", "Total", "total")
    for col in "BCD":
        s.set(f"{col}{end + 1}", "", "total")
    s.set(f"E{end + 1}", None, "totalmoney", "money", f"of:=SUM([.E{head + 1}:.E{end}])")
    s.set(f"G{head}", "Category", "head")
    s.set(f"H{head}", "Spent", "headr")
    cats = ["Food", "Bills", "Travel", "Eating out", "Going out", "Clothes", "Home", "Other"]
    for i, c in enumerate(cats):
        r = head + 1 + i
        s.set(f"G{r}", c)
        s.set(f"H{r}", None, "money", "money", f"of:=SUMIF([.$C${head + 1}:.$C${end}];[.G{r}];[.$E${head + 1}:.$E${end}])")
    s.set(f"G{head + 1 + len(cats)}", "Total", "total")
    s.set(f"H{head + 1 + len(cats)}", None, "totalmoney", "money", f"of:=SUM([.H{head + 1}:.H{head + len(cats)}])")
    # A drop-down of the categories on the right, in every Category cell.
    validation = (f'<table:content-validations><table:content-validation table:name="cats" '
                  f'table:condition="of:cell-content-is-in-list([.$G${head + 1}:.$G${head + len(cats)}])" '
                  f'table:allow-empty-cell="true" table:display-list="unsorted" table:base-cell-address="&apos;Expenses&apos;.C{head + 1}">'
                  f'<table:error-message table:display="false" table:message-type="stop"/></table:content-validation></table:content-validations>')
    return fods("Expenses", [s], calc, validations=validation), (head + 1, end)


def calc_default():
    calc = Calc()
    s = Sheet("Sheet1", [2.26])
    return fods("Spreadsheet", [s], calc)


def add_validation(xml, first, last):
    """Puts the category drop-down on column C of the expense rows."""
    rows = xml.split("<table:table-row ")
    for n in range(first, last + 1):
        row = rows[n]
        cells = row.split("<table:table-cell ")
        cells[3] = 'table:content-validation-name="cats" ' + cells[3]
        rows[n] = "<table:table-cell ".join(cells)
    return "<table:table-row ".join(rows)


# Output

WRITE = [
    ("letter", letter),
    ("cv", cv),
    ("report", report),
    ("newsletter", newsletter),
    ("meeting-notes", meeting_notes),
]
TABLES = [
    ("budget", budget),
    ("invoice", invoice),
    ("planner", planner),
    ("timesheet", timesheet),
    ("expenses", expenses),
]


def run(cmd, **kw):
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, **kw)


def convert(src, fmt, outdir, profile):
    run(["soffice", f"-env:UserInstallation=file://{profile}", "--headless", "--norestore",
         "--convert-to", fmt, "--outdir", outdir, src])


def thumbnail(pdf, png):
    """The first page at the card's 2x size, cut to the card's shape
    (134 by 176) from the top left, so the heading stays in view; a
    landscape page keeps its full height and shows its left side."""
    base = png[:-4]
    run(["pdftoppm", "-png", "-r", "110", "-f", "1", "-l", "1", "-singlefile", pdf, base])
    size = subprocess.run(["identify", "-format", "%w %h", png], check=True, capture_output=True, text=True).stdout.split()
    fit = "x352" if int(size[0]) > int(size[1]) else "268x"
    run(["convert", png, "-resize", fit, "-gravity", "northwest", "-crop", "268x352+0+0", "+repage",
         "-strip", "-define", "png:compression-level=9", png])


# Formulas in a flat file come without results. LibreOffice works them out
# on load only when told to, and the .ots then keeps the results; the
# setting goes into the profile after a first start has made it.
RECALC = (
    '<item oor:path="/org.openoffice.Office.Calc/Formula/Load">'
    '<prop oor:name="ODFRecalcMode" oor:op="fuse"><value>0</value></prop></item>'
)


def main():
    work = tempfile.mkdtemp()
    profile = os.path.join(work, "profile")
    run(["soffice", f"-env:UserInstallation=file://{profile}", "--headless", "--norestore", "--terminate_after_init"])
    xcu = os.path.join(profile, "user", "registrymodifications.xcu")
    with open(xcu, encoding="utf-8") as f:
        registry = f.read()
    with open(xcu, "w", encoding="utf-8") as f:
        f.write(registry.replace("</oor:items>", RECALC + "</oor:items>"))
    for folder, items, ext, filt in (("write", WRITE, "fodt", "ott"), ("tables", TABLES, "fods", "ots")):
        dest = os.path.join(HERE, folder)
        os.makedirs(dest, exist_ok=True)
        for name, make in items:
            content = make()
            if isinstance(content, tuple):
                content, (first, last) = content
                content = add_validation(content, first, last)
            src = os.path.join(work, f"{name}.{ext}")
            with open(src, "w", encoding="utf-8") as f:
                f.write(content)
            convert(src, filt, dest, profile)
            if filt == "ots":
                # A flat file's formulas are only worked out once it has
                # been through LibreOffice, so the .ots goes round again.
                again = os.path.join(work, "again")
                convert(os.path.join(dest, f"{name}.{filt}"), filt, again, profile)
                shutil.move(os.path.join(again, f"{name}.{filt}"), os.path.join(dest, f"{name}.{filt}"))
            convert(os.path.join(dest, f"{name}.{filt}"), "pdf", work, profile)
            thumbnail(os.path.join(work, f"{name}.pdf"), os.path.join(dest, f"{name}.png"))
            print(f"{folder}/{name}.{filt}")
    dest = os.path.join(HERE, "default")
    os.makedirs(dest, exist_ok=True)
    for name, make, ext, filt in (("Default", writer_default, "fodt", "ott"), ("Default", calc_default, "fods", "ots")):
        src = os.path.join(work, f"{name}.{ext}")
        with open(src, "w", encoding="utf-8") as f:
            f.write(make())
        convert(src, filt, dest, profile)
        print(f"default/{name}.{filt}")
    if "--keep" not in sys.argv:
        shutil.rmtree(work)
    else:
        print(work)


if __name__ == "__main__":
    main()
