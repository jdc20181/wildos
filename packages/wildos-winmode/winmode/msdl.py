"""An official Windows 11 download link and its SHA-256, from Microsoft.

The one module that knows how Microsoft's download page works, because
Microsoft changes it without notice (brief risk 10.3). The request sequence
follows Mido (github.com/ElliotKillick/Mido, GPL-3.0) as maintained in
dockur/windows src/mido.sh (MIT), including the ov-df challenge Microsoft
added in 2025.
"""

import json
import re
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid

PAGE = "https://www.microsoft.com/en-us/software-download/windows11"
PROFILE = "606624d44113"
ORG_ID = "y6jn8c31"
OV_INSTANCE = "560dc9f3-1aa5-4a2f-b63c-9e18f8d0e175"
# Microsoft refuses requests from obvious scripts; a current desktop browser
# string is what Mido and dockur send.
AGENT = "Mozilla/5.0 (X11; Linux x86_64; rv:140.0) Gecko/20100101 Firefox/140.0"


class Refused(RuntimeError):
    """Microsoft blocked the automated download (usually by address): the
    person can download the ISO in a browser instead (board 9.7)."""


class Changed(RuntimeError):
    """Microsoft's page no longer looks the way this module expects."""


def language_name(bcp47):
    """The name Microsoft uses for a display language, in both the SKU list
    and the checksum table."""
    tag = bcp47 or ""
    exact = {
        "en-GB": "English International", "en-IE": "English International", "en-AU": "English International",
        "en-NZ": "English International", "en-IN": "English International", "en-ZA": "English International",
        "en-US": "English", "en": "English", "es-MX": "Spanish (Mexico)", "fr-CA": "French Canadian",
        "pt-BR": "Brazilian Portuguese", "zh-TW": "Chinese Traditional", "zh-HK": "Chinese Traditional",
    }
    if tag in exact:
        return exact[tag]
    prefix = {
        "ar": "Arabic", "bg": "Bulgarian", "cs": "Czech", "da": "Danish", "de": "German", "el": "Greek",
        "es": "Spanish", "et": "Estonian", "fi": "Finnish", "fr": "French", "he": "Hebrew", "hr": "Croatian",
        "hu": "Hungarian", "it": "Italian", "ja": "Japanese", "ko": "Korean", "lt": "Lithuanian", "lv": "Latvian",
        "nb": "Norwegian", "nn": "Norwegian", "no": "Norwegian", "nl": "Dutch", "pl": "Polish", "pt": "Portuguese",
        "ro": "Romanian", "ru": "Russian", "sk": "Slovak", "sl": "Slovenian", "sr": "Serbian Latin",
        "sv": "Swedish", "th": "Thai", "tr": "Turkish", "uk": "Ukrainian", "zh": "Chinese Simplified",
    }
    return prefix.get(tag.split("-")[0], "English International")


def _get(url, referer=None, limit=2 * 1024 * 1024):
    req = urllib.request.Request(url, headers={"User-Agent": AGENT, "Accept": ""})
    if referer:
        req.add_header("Referer", referer)
    with urllib.request.urlopen(req, timeout=60) as r:
        data = r.read(limit + 1)
    if len(data) > limit:
        raise Changed(f"{urllib.parse.urlsplit(url).netloc} sent more than expected")
    return data.decode("utf-8", "replace")


def checksums(page=None):
    """Every (language, sha256) pair in the page's "Verify your download"
    table, for checking an ISO downloaded in a browser too."""
    page = page if page is not None else _get(PAGE)
    flat = re.sub(r"[\r\n]", " ", page)
    out = []
    for name, sha in re.findall(r"<td>([^<]*)</td>\s*<td>([A-Fa-f0-9]{64})</td>", flat):
        out.append((re.sub(r"  +", " ", name).strip(), sha.lower()))
    return out


def fetch(bcp47):
    """{url, file, sha256, language, product_edition_id, fetched}."""
    language = language_name(bcp47)
    try:
        page = _get(PAGE)
    except (urllib.error.URLError, OSError) as e:
        raise Changed(f"couldn't load Microsoft's Windows 11 download page ({e})") from e
    flat = re.sub(r"[\r\n]", " ", page)
    m = re.search(r"<option[^>]*value=[\"']([0-9]+)[\"'][^>]*>\s*Windows 11[^<]*x64", flat, re.I)
    if not m:
        raise Changed("the download page no longer lists a Windows 11 x64 multi-edition ISO")
    edition_id = m.group(1)[:16]
    sha = next((s for name, s in checksums(page) if name == f"{language} 64-bit"), None)
    if not sha:
        raise Changed(f'the download page has no SHA-256 for "{language} 64-bit"')

    session = str(uuid.uuid4())
    try:
        _get(f"https://vlscppe.microsoft.com/tags?org_id={ORG_ID}&session_id={session}", limit=100 * 1024)
        ov = _get(f"https://ov-df.microsoft.com/mdt.js?instanceId={OV_INSTANCE}&PageId=si&session_id={session}", limit=1024 * 1024)
    except (urllib.error.URLError, OSError) as e:
        raise Changed(f"Microsoft's session service refused the session ({e})") from e
    token = re.search(r"[?&]w=([A-Fa-f0-9]+)", ov)
    ticks = re.search(r'rticks="\+?([0-9]+)', ov)
    if not token or not ticks:
        raise Changed("Microsoft's ov-df challenge changed shape")
    time.sleep(0.3)
    now = int(time.time() * 1000)
    try:
        _get(f"https://ov-df.microsoft.com/?session_id={session}&CustomerId={OV_INSTANCE}&PageId=si"
             f"&w={token.group(1)}&mdt={now}&rticks={ticks.group(1)}", limit=100 * 1024)
        skus = json.loads(_get(
            "https://www.microsoft.com/software-download-connector/api/getskuinformationbyproductedition"
            f"?profile={PROFILE}&ProductEditionId={edition_id}&SKU=undefined&friendlyFileName=undefined"
            f"&Locale=en-US&sessionID={session}", referer=PAGE, limit=100 * 1024))
    except (urllib.error.URLError, OSError, ValueError) as e:
        raise Changed(f"Microsoft's list of languages didn't load ({e})") from e
    sku_id = next((s.get("Id") for s in skus.get("Skus", []) or []
                   if language in (s.get("Language"), s.get("LocalizedLanguage"))), None)
    if not sku_id:
        raise Changed(f'Microsoft offered no "{language}" download for edition {edition_id}')

    # If Microsoft blocks anything it is this last call, usually by address.
    try:
        text = _get("https://www.microsoft.com/software-download-connector/api/GetProductDownloadLinksBySku"
                    f"?profile={PROFILE}&ProductEditionId=undefined&SKU={sku_id}&friendlyFileName=undefined"
                    f"&Locale=en-US&sessionID={session}", referer=PAGE, limit=100 * 1024)
    except urllib.error.HTTPError as e:
        raise Refused(f"Microsoft refused the download link ({e.code})") from e
    except (urllib.error.URLError, OSError) as e:
        raise Changed(f"Microsoft's download link request failed ({e})") from e
    if "Sentinel marked this request as rejected" in text or "We are unable to complete your request" in text:
        raise Refused("Microsoft blocked the automated download from this address")
    try:
        options = json.loads(text).get("ProductDownloadOptions", []) or []
    except ValueError as e:
        raise Changed("Microsoft's download link answer isn't JSON") from e
    url = next((o.get("Uri") for o in options if o.get("DownloadType") == 1), None)
    url = url or next((o.get("Uri") for o in options if "x64" in (o.get("Uri") or "").lower()), None)
    host = urllib.parse.urlsplit(url or "").netloc
    if not url or not url.startswith("https://") or not (host == "microsoft.com" or host.endswith(".microsoft.com")):
        raise Changed("Microsoft returned no x64 link, or one not on a Microsoft host")
    return {"url": url, "file": urllib.parse.unquote(url.split("?")[0].rsplit("/", 1)[-1]), "sha256": sha,
            "language": language, "product_edition_id": edition_id, "fetched": int(time.time())}
