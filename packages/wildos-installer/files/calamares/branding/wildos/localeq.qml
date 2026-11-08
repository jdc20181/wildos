/* The time zone module has no page of its own (passThrough in localeq.conf):
 * the Language and keyboard page shows and changes it through this
 * configuration object.
 */
import WildOS.Installer

import QtQuick

Item {
    Component.onCompleted: Installer.locale = config
}
