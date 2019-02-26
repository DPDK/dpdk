# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# collection of static data

# keycode constants
CTRL_A = chr(1)
CTRL_B = chr(2)
CTRL_C = chr(3)
CTRL_D = chr(4)
CTRL_E = chr(5)
CTRL_F = chr(6)
CTRL_K = chr(11)
CTRL_L = chr(12)
CTRL_N = chr(14)
CTRL_P = chr(16)
CTRL_W = chr(23)
CTRL_Y = chr(25)
ALT_B = chr(27) + chr(98)
ALT_D = chr(27) + chr(100)
ALT_F = chr(27) + chr(102)
ALT_BKSPACE = chr(27) + chr(127)
DEL = chr(27) + chr(91) + chr(51) + chr(126)
TAB = chr(9)
HELP = chr(63)
BKSPACE = chr(127)
RIGHT = chr(27) + chr(91) + chr(67)
DOWN = chr(27) + chr(91) + chr(66)
LEFT = chr(27) + chr(91) + chr(68)
UP = chr(27) + chr(91) + chr(65)
ENTER2 = '\r'
ENTER = '\n'

# expected result constants
NOT_FOUND = "Command not found"
BAD_ARG = "Bad arguments"
AMBIG = "Ambiguous command"
CMD1 = "Command 1 parsed!"
CMD2 = "Command 2 parsed!"
SINGLE = "Single word command parsed!"
SINGLE_LONG = "Single long word command parsed!"
AUTO1 = "Autocomplete command 1 parsed!"
AUTO2 = "Autocomplete command 2 parsed!"

# misc defines
CMD_QUIT = "quit"
CMD_GET_BUFSIZE = "get_history_bufsize"
BUFSIZE_TEMPLATE = "History buffer size: "
PROMPT = "CMDLINE_TEST>>"

# test defines
# each test tests progressively diverse set of keys. this way for example
# if we want to use some key sequence in the test, we first need to test
# that it itself does what it is expected to do. Most of the tests are
# designed that way.
#
# example: "arrows & delete test 1". we enter a partially valid command,
# then move 3 chars left and use delete three times. this way we get to
# know that "delete", "left" and "ctrl+B" all work (because if any of
# them fails, the whole test will fail and next tests won't be run).
#
# each test consists of name, character sequence to send to child,
# and expected output (if any).

tests = [
    # test basic commands
    {"Name": "command test 1",
     "Sequence": "ambiguous first" + ENTER,
     "Result": CMD1},
    {"Name": "command test 2",
     "Sequence": "ambiguous second" + ENTER,
     "Result": CMD2},
    {"Name": "command test 3",
     "Sequence": "ambiguous ambiguous" + ENTER,
     "Result": AMBIG},
    {"Name": "command test 4",
     "Sequence": "ambiguous ambiguous2" + ENTER,
     "Result": AMBIG},

    {"Name": "invalid command test 1",
     "Sequence": "ambiguous invalid" + ENTER,
     "Result": BAD_ARG},
    # test invalid commands
    {"Name": "invalid command test 2",
     "Sequence": "invalid" + ENTER,
     "Result": NOT_FOUND},
    {"Name": "invalid command test 3",
     "Sequence": "ambiguousinvalid" + ENTER2,
     "Result": NOT_FOUND},

    # test arrows and deletes
    {"Name": "arrows & delete test 1",
     "Sequence": "singlebad" + LEFT*2 + CTRL_B + DEL*3 + ENTER,
     "Result": SINGLE},
    {"Name": "arrows & delete test 2",
     "Sequence": "singlebad" + LEFT*5 + RIGHT + CTRL_F + DEL*3 + ENTER,
     "Result": SINGLE},

    # test backspace
    {"Name": "backspace test",
     "Sequence": "singlebad" + BKSPACE*3 + ENTER,
     "Result": SINGLE},

    # test goto left and goto right
    {"Name": "goto left test",
     "Sequence": "biguous first" + CTRL_A + "am" + ENTER,
     "Result": CMD1},
    {"Name": "goto right test",
     "Sequence": "biguous fir" + CTRL_A + "am" + CTRL_E + "st" + ENTER,
     "Result": CMD1},

    # test goto words
    {"Name": "goto left word test",
     "Sequence": "ambiguous st" + ALT_B + "fir" + ENTER,
     "Result": CMD1},
    {"Name": "goto right word test",
     "Sequence": "ambig first" + CTRL_A + ALT_F + "uous" + ENTER,
     "Result": CMD1},

    # test removing words
    {"Name": "remove left word 1",
     "Sequence": "single invalid" + CTRL_W + ENTER,
     "Result": SINGLE},
    {"Name": "remove left word 2",
     "Sequence": "single invalid" + ALT_BKSPACE + ENTER,
     "Result": SINGLE},
    {"Name": "remove right word",
     "Sequence": "single invalid" + ALT_B + ALT_D + ENTER,
     "Result": SINGLE},

    # test kill buffer (copy and paste)
    {"Name": "killbuffer test 1",
     "Sequence": "ambiguous" + CTRL_A + CTRL_K + " first" + CTRL_A +
                 CTRL_Y + ENTER,
     "Result": CMD1},
    {"Name": "killbuffer test 2",
     "Sequence": "ambiguous" + CTRL_A + CTRL_K + CTRL_Y*26 + ENTER,
     "Result": NOT_FOUND},

    # test newline
    {"Name": "newline test",
     "Sequence": "invalid" + CTRL_C + "single" + ENTER,
     "Result": SINGLE},

    # test redisplay (nothing should really happen)
    {"Name": "redisplay test",
     "Sequence": "single" + CTRL_L + ENTER,
     "Result": SINGLE},

    # test autocomplete
    {"Name": "autocomplete test 1",
     "Sequence": "si" + TAB + ENTER,
     "Result": SINGLE},
    {"Name": "autocomplete test 2",
     "Sequence": "si" + TAB + "_" + TAB + ENTER,
     "Result": SINGLE_LONG},
    {"Name": "autocomplete test 3",
     "Sequence": "in" + TAB + ENTER,
     "Result": NOT_FOUND},
    {"Name": "autocomplete test 4",
     "Sequence": "am" + TAB + ENTER,
     "Result": BAD_ARG},
    {"Name": "autocomplete test 5",
     "Sequence": "am" + TAB + "fir" + TAB + ENTER,
     "Result": CMD1},
    {"Name": "autocomplete test 6",
     "Sequence": "am" + TAB + "fir" + TAB + TAB + ENTER,
     "Result": CMD1},
    {"Name": "autocomplete test 7",
     "Sequence": "am" + TAB + "fir" + TAB + " " + TAB + ENTER,
     "Result": CMD1},
    {"Name": "autocomplete test 8",
     "Sequence": "am" + TAB + "     am" + TAB + "   " + ENTER,
     "Result": AMBIG},
    {"Name": "autocomplete test 9",
     "Sequence": "am" + TAB + "inv" + TAB + ENTER,
     "Result": BAD_ARG},
    {"Name": "autocomplete test 10",
     "Sequence": "au" + TAB + ENTER,
     "Result": NOT_FOUND},
    {"Name": "autocomplete test 11",
     "Sequence": "au" + TAB + "1" + ENTER,
     "Result": AUTO1},
    {"Name": "autocomplete test 12",
     "Sequence": "au" + TAB + "2" + ENTER,
     "Result": AUTO2},
    {"Name": "autocomplete test 13",
     "Sequence": "au" + TAB + "2" + TAB + ENTER,
     "Result": AUTO2},
    {"Name": "autocomplete test 14",
     "Sequence": "au" + TAB + "2   " + TAB + ENTER,
     "Result": AUTO2},
    {"Name": "autocomplete test 15",
     "Sequence": "24" + TAB + ENTER,
     "Result": "24"},

    # test history
    {"Name": "history test 1",
     "Sequence": "invalid" + ENTER + "single" + ENTER + "invalid" +
                 ENTER + UP + CTRL_P + ENTER,
     "Result": SINGLE},
    {"Name": "history test 2",
     "Sequence": "invalid" + ENTER + "ambiguous first" + ENTER + "invalid" +
                 ENTER + "single" + ENTER + UP * 3 + CTRL_N + DOWN + ENTER,
     "Result": SINGLE},

    #
    # tests that improve coverage
    #

    # empty space tests
    {"Name": "empty space test 1",
     "Sequence": RIGHT + LEFT + CTRL_B + CTRL_F + ENTER,
     "Result": PROMPT},
    {"Name": "empty space test 2",
     "Sequence": BKSPACE + ENTER,
     "Result": PROMPT},
    {"Name": "empty space test 3",
     "Sequence": CTRL_E*2 + CTRL_A*2 + ENTER,
     "Result": PROMPT},
    {"Name": "empty space test 4",
     "Sequence": ALT_F*2 + ALT_B*2 + ENTER,
     "Result": PROMPT},
    {"Name": "empty space test 5",
     "Sequence": " " + CTRL_E*2 + CTRL_A*2 + ENTER,
     "Result": PROMPT},
    {"Name": "empty space test 6",
     "Sequence": " " + CTRL_A + ALT_F*2 + ALT_B*2 + ENTER,
     "Result": PROMPT},
    {"Name": "empty space test 7",
     "Sequence": "  " + CTRL_A + CTRL_D + CTRL_E + CTRL_D + ENTER,
     "Result": PROMPT},
    {"Name": "empty space test 8",
     "Sequence": " space" + CTRL_W*2 + ENTER,
     "Result": PROMPT},
    {"Name": "empty space test 9",
     "Sequence": " space" + ALT_BKSPACE*2 + ENTER,
     "Result": PROMPT},
    {"Name": "empty space test 10",
     "Sequence": " space " + CTRL_A + ALT_D*3 + ENTER,
     "Result": PROMPT},

    # non-printable char tests
    {"Name": "non-printable test 1",
     "Sequence": chr(27) + chr(47) + ENTER,
     "Result": PROMPT},
    {"Name": "non-printable test 2",
     "Sequence": chr(27) + chr(128) + ENTER*7,
     "Result": PROMPT},
    {"Name": "non-printable test 3",
     "Sequence": chr(27) + chr(91) + chr(127) + ENTER*6,
     "Result": PROMPT},

    # miscellaneous tests
    {"Name": "misc test 1",
     "Sequence": ENTER,
     "Result": PROMPT},
    {"Name": "misc test 2",
     "Sequence": "single #comment" + ENTER,
     "Result": SINGLE},
    {"Name": "misc test 3",
     "Sequence": "#empty line" + ENTER,
     "Result": PROMPT},
    {"Name": "misc test 4",
     "Sequence": "   single  " + ENTER,
     "Result": SINGLE},
    {"Name": "misc test 5",
     "Sequence": "single#" + ENTER,
     "Result": SINGLE},
    {"Name": "misc test 6",
     "Sequence": 'a' * 257 + ENTER,
     "Result": NOT_FOUND},
    {"Name": "misc test 7",
     "Sequence": "clear_history" + UP*5 + DOWN*5 + ENTER,
     "Result": PROMPT},
    {"Name": "misc test 8",
     "Sequence": "a" + HELP + CTRL_C,
     "Result": PROMPT},
    {"Name": "misc test 9",
     "Sequence": CTRL_D*3,
     "Result": None},
]
