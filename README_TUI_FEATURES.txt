=== AUTOCOPY TUI MODE FEATURES ===

Version: 0.0.5
Date: 30.01.2026

NEW FEATURES IN TUI MODE:
========================

1. WHITE HIGHLIGHT BAR
   - Current selected item in the log is highlighted with a white background
   - Makes it easy to see which item you are selecting
   - Position starts at the most recent (last) item added

2. ARROW KEY NAVIGATION
   - UP ARROW: Move selection up one item (to older items)
   - DOWN ARROW: Move selection down one item (to newer items)
   - Auto-scrolling: View automatically scrolls when needed
   - When no item is selected, UP selects the newest, DOWN selects the oldest

3. CTRL+ENTER TO COPY SELECTED ITEM
   - Press CTRL+ENTER while an item is highlighted
   - The selected item's text is copied to system clipboard
   - Clipboard is cleared first, then filled with the selected text
   - Confirmation: Text appears in system clipboard (can be pasted anywhere)

USAGE EXAMPLE:
==============
./autocopy.exe --tui --1click --force

Then:
1. Copy texts by selecting them in other windows (single click)
2. Items appear in the TUI log area
3. Use UP/DOWN arrows to navigate the list
4. White bar shows current selection
5. Press CTRL+ENTER to copy selected item to clipboard
6. Press CTRL+Z to exit

NAVIGATION:
===========
- UP ARROW: Previous item (older)
- DOWN ARROW: Next item (newer)
- CTRL+ENTER: Copy selected item to clipboard
- CTRL+Z: Exit application
