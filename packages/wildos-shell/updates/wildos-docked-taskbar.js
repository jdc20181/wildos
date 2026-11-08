// The taskbar is docked by default now (a 52 px bar along the bottom). A
// panel set up before that is the 76 px floating one; Plasma runs this once
// per account, which moves it to the docked height to match the new
// default. Settings > Personalisation switches back to floating.
panels().forEach(function (p) {
    if (p.location === "bottom" && p.height === 76) {
        p.height = 52;
    }
});
