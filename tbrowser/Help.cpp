#include "Help.hpp"

const char* HelpContents =
//345678901234567890123456789012345678901234567890123456789012345678901234567890
"\n"
"                            Usenet Archive Browser\n"
"                           ========================\n"
"\n"
"                                  Thread view\n"
"                                 -------------\n"
"\n"
"  Each message in archive is displayed in a separate line. An example line\n"
"would look like this:\n"
"\n"
"  -  42 [Author's name] + Subject of the message          [1995-07-21 14:32]\n"
"  ^   ^                 ^\n"
"  |   |                 `-- Indicates folded thread tree.\n"
"  |   `-------------------- Number of messages in this subtree.\n"
"  `------------------------ Message flags.\n"
"\n"
"  Folded thread trees are indicated with '+' sign. Expanded trees are marked\n"
"with '-'. There is no marker for messages without children.\n"
"\n"
"  The following message flags may be displayed:\n"
"   '-' - indicates no flags set,\n"
"   'r' - this message was already visited (and presumably, read),\n"
"   'R' - this message and all its children were visited.\n"
"\n"
"  Note that messages are tracked by their identifiers, so it is perfectly\n"
"normal to enter a previously unvisited newsgroup and have some messages already\n"
"marked as read. This is due to crossposting of the same message on two or more\n"
"groups.\n"
"\n"
"  Currently selected message is indicated by the '->' cursor indicator on the\n"
"left side of the screen. Complementary marker '<' is also displayed on the right\n"
"end. Notice that currently displayed message and message marked by cursor may be\n"
"different.\n"
"\n"
"                                  Keybindings\n"
"                                 -------------\n"
"\n"
"                 q: quit\n"
"    up/down arrows: move cursor\n"
"      page up/down: scroll thread view up/down\n"
"          home/end: move cursor to top/bottom of the screen\n"
"       right arrow: expand thread tree OR move cursor to next message\n"
"        left arrow: collapse thread tree OR move cursor to previous message\n"
"                 x: collapse or expand thread tree\n"
"                 e: collapse whole thread and close message view\n"
"                 d: mark message as read and move cursor to next unread one\n"
"             enter: view selected message OR scroll message down one line\n"
"         backspace: scroll message up one line\n"
"             space: view selected message OR scroll message down one screen\n"
"            delete: scroll message up one screen\n"
"                 ,: move cursor to previously viewed message\n"
"                 .: reversal of the above\n"
"                 c: view group charter\n"
"                 t: switch between abbreviated and full headers list\n"
"                 r: enable/disable ROT13 encoding\n"
"                 g: go to specified message id\n"
"                 s: search messages for specified text\n"
"\n"
"                                  Search view\n"
"                                 -------------\n"
"\n"
"  Going to search view will display previous query results. This way you can\n"
"quickly view messages without losing state of search."
;
