import re
from gdb.printing import PrettyPrinter, register_pretty_printer
import gdb
import uuid

class SymExpr(object):
    def __init__(self, val):
        self.val = val

    @property
    def expr_id(self):
        return (int(self.val) >> 8)

    @property
    def bits(self):
        return (int(self.val) & 0xff)

    def to_string(self):
        return f"Expr{self.bits}[{hex(self.expr_id)}]"

class StridedSymExprList(object):
    def __init__(self, start_id=None, stride=None, n=None, size=None):
        self.start_id = start_id
        self.stride = stride
        self.n = n
        self.size = size

    def __repr__(self):
        return f"StridedExpr(start={hex(self.start_id)}, stride={self.stride}, n={self.n}, size={self.size})"

    def __str__(self):
        return ' .. '.join(f"Expr{self.size}[{hex(self.start_id + self.stride * i)}]" for i in range(self.n))


class SymCCPrettyPrinterLocator(PrettyPrinter):
    """Given a gdb.Value, search for a custom pretty printer"""

    def __init__(self):
        super(SymCCPrettyPrinterLocator, self).__init__(
            "symcc_pretty_printers", []
        )

    def __call__(self, val):
        """Return the custom formatter if the type can be handled"""

        typename = gdb.types.get_basic_type(val.type).tag
        if typename is None:
            typename = val.type.name

        if typename == "SymExpr":
            return SymExpr(val)

register_pretty_printer(None, SymCCPrettyPrinterLocator(), replace=True)


class SymExprDumpCmd(gdb.Command):
    """Prints the ListNode from our example in a nice format!"""

    def __init__(self):
        super(SymExprDumpCmd, self).__init__(
            "symexpr_dump", gdb.COMMAND_USER
        )

    def complete(self, text, word):
        # We expect the argument passed to be a symbol so fallback to the
        # internal tab-completion handler for symbols
        return gdb.COMPLETE_SYMBOL

    def get_symbolic_expressions(self, shadow_page_start):
        ptr = shadow_page_start.cast(gdb.lookup_type("SymExpr").pointer())
        for i in range(0x1000):
            val = ptr.dereference()
            if int(val) != 0:
                yield (i, SymExpr(ptr.dereference()))
            ptr += 1

    def get_symbolic_expression_ranges(self, sym_vals):
        cur = []
        last_addr = None
        cur_starting_addr = None
        for addr, val in sorted(sym_vals):
            if cur_starting_addr is None:
                cur_starting_addr = addr
            if last_addr is None or last_addr + 1 == addr:
                cur.append(val)
            else:
                yield (cur_starting_addr, cur)
                cur_starting_addr = addr
                cur = [val]
            last_addr = addr

    def _get_strided_expr(self, consecutive_exprs):
        '''
        Convert sequences of ranged consecutive exprs into a range summary

        E.g.:
        Expr(8, 8) .. Expr(9, 8) .. Expr(10, 8) .. Expr(11, 8)      -> StridedExpr(start=8, stride=1, n=4, size=8)
        Expr(8, 8) .. Expr(10, 8) .. Expr(12, 8) .. Expr(14, 8)      -> StridedExpr(start=8, stride=2, n=4, size=8)
        Expr(8, 8) .. Expr(12, 8) .. Expr(16, 8) .. Expr(20, 8)      -> StridedExpr(start=8, stride=4, n=4, size=8)

        Expr(20, 8) .. Expr(16, 8) .. Expr(12, 8) .. Expr(8, 8)      -> StridedExpr(start=8, stride=-4, n=4, size=8)
        etc.
        '''
        if len(consecutive_exprs) == 1:
            return consecutive_exprs[0], 1

        stride = consecutive_exprs[1].expr_id - consecutive_exprs[0].expr_id
        if abs(stride) not in [1, 2, 4, 8, 16]:
            return consecutive_exprs[0], 1

        for i in range(2, len(consecutive_exprs)):
            if consecutive_exprs[i].expr_id - consecutive_exprs[i-1].expr_id != stride:
                return StridedSymExprList(start_id=consecutive_exprs[0].expr_id, stride=stride, n=i, size=consecutive_exprs[0].bits), i

        else:
            return StridedSymExprList(start_id=consecutive_exprs[0].expr_id, stride=stride, n=len(consecutive_exprs), size=consecutive_exprs[0].bits), len(consecutive_exprs)

    def summarize_strided_exprs(self, addrs_and_exprs):
        i = 0
        while i < len(addrs_and_exprs):
            expr, n = self._get_strided_expr(addrs_and_exprs[i:])
            yield i, expr
            i += n

    def invoke(self, args, from_tty):
        # We can pass args here and use Python CLI utilities like argparse
        # to do argument parsing
        # print("Args Passed: %s" % args)

        # first, get the original language flag
        orig_lang = gdb.execute("show language", to_string=True)
        orig_lang = orig_lang.split("The current source language is ")[1].split('"')[1].split('; ')[0]
        print(repr(orig_lang))

        gdb.execute("set language c++")

        # get the g_shadow_pages value
        g_shadow_pages = gdb.parse_and_eval("g_shadow_pages")

        '''
        Parse out addresses from this output
        std::map with 2 elements = {
          [0x631000030000] = 0x62d00000a400,
          [0x7ffdb1f1e000] = 0x62d000000400
        }'''

        # import ipdb; ipdb.set_trace()
        s_val = str(g_shadow_pages)
        s_val = re.findall(r"\[(0x[0-9a-f]+)\] = (0x[0-9a-f]+)", s_val)
        s_val = [(int(x, 16), int(y, 16)) for x, y in s_val]
        # print(g_shadow_pages)
        # print([f'{hex(a)}->{hex(b)}' for a, b in s_val])
        for real_start, shadow_start in s_val:
            print(f'Page {hex(real_start)}')
            addr_and_val = [(real_start + page_byte_offset, val)
                    for page_byte_offset, val
                    in self.get_symbolic_expressions(gdb.Value(shadow_start))
                ]

            for addr, values in self.get_symbolic_expression_ranges(addr_and_val):
                print("Start: %s" % hex(addr))
                for offset, val in self.summarize_strided_exprs(values):
                    print(f"    +{hex(offset)}: {str(val)}")

        gdb.execute(f"set language {orig_lang}")


SymExprDumpCmd()

