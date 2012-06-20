
// cursor addressing based on the old Newbury Data Recording 9000 series dumb terminal

// Cursor positioning is done with [0x16][0x20+row][0x20+col]
// All other codes are single control characters


#define TERM_CPC     0x16     /**< Cursor position prefix - followed by row + column */
#define TERM_ROW     0x20     /**< cursor position row offset */
#define TERM_COL     0x20     /**< cursor position column offset */
#define TERM_CLR     0x1f     /**< Clear screen */
#define TERM_HOME    0x1d     /**< Home */
#define TERM_UP      0x0b     /**< Cursor up */
#define TERM_DOWN    0x0a     /**< Cursor down */
#define TERM_LEFT    0x08     /**< Cursor left */
#define TERM_RIGHT   0x18     /**< Cursor right */

#define TERM_STATE_NORMAL   0x00    /**< state that indicates we're passing data straight through */
#define TERM_STATE_ROW      0x01    /**< state that indicates we're waiting for the row address */
#define TERM_STATE_COL      0x02    /**< state that indicates we're waiting for the column address */
