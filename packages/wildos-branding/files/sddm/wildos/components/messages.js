.pragma library

// PAM modules talk in their own jargon ("faillock: Error opening the tally
// file", "Authentication token manipulation error"), so nothing they say is
// ever shown as it is. Each message is either recognised as a reason the
// person cannot sign in right now, or dropped.

const wrongPassword = "That password did not work";
const unavailable = "You can't sign in right now. Try again in a moment.";

const blockingPatterns = [
    /lock(ed)?\b.*\b(account|user)|account.*\block/i,
    /too many|maximum number of|retry later|try again later|temporarily/i,
    /expired|disabled|not allowed|access denied|denied access/i,
];

// Returns the plain sentence to show for a PAM error or info message, or an
// empty string when it should not be shown at all.
function translate(message) {
    if (!message)
        return "";
    const text = String(message);
    for (const pattern of blockingPatterns) {
        if (pattern.test(text))
            return unavailable;
    }
    return "";
}
