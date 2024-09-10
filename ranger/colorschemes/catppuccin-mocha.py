from ranger.gui.colorscheme import ColorScheme
from ranger.gui.color import default_colors, reverse, bold, normal, default

class MochaTheme(ColorScheme):
    progress_bar_color = 151  # Mocha Green

    def use(self, context):
        fg, bg, attr = default_colors

        if context.reset:
            return default_colors

        elif context.in_browser:
            if context.selected:
                attr = reverse
            else:
                attr = normal
            if context.empty or context.error:
                fg = 211  # Mocha Red
                bg = 59   # Mocha Surface0
            if context.border:
                fg = 60   # Mocha Overlay0
            if context.image:
                fg = 111  # Mocha Blue
            if context.video:
                fg = 211  # Mocha Red
            if context.audio:
                fg = 151  # Mocha Green
            if context.document:
                fg = 218  # Mocha Pink
            if context.container:
                attr |= bold
                fg = 211  # Mocha Red
            if context.directory:
                attr |= bold
                fg = 111  # Mocha Blue
            elif context.executable and not \
                    any((context.media, context.container,
                         context.fifo, context.socket)):
                attr |= bold
                fg = 151  # Mocha Green
            if context.socket:
                fg = 189  # Mocha Text
                attr |= bold
            if context.fifo or context.device:
                fg = 229  # Mocha Yellow
                if context.device:
                    attr |= bold
            if context.link:
                fg = 111 if context.good else 151  # Mocha Blue / Mocha Green
                bg = 59   # Mocha Surface0
            if context.bad:
                bg = 59   # Mocha Surface0
            if context.tag_marker and not context.selected:
                attr |= bold
                if fg in (211, 95):  # Adjust if needed
                    fg = 60  # Mocha Overlay0
                else:
                    fg = 211  # Mocha Red
            if not context.selected and (context.cut or context.copied):
                fg = 151  # Mocha Green
                bg = 59   # Mocha Surface0
            if context.main_column:
                if context.selected:
                    attr |= bold
                if context.marked:
                    attr |= bold
                    fg = 111  # Mocha Blue
            if context.badinfo:
                if attr & reverse:
                    bg = 211  # Mocha Red
                else:
                    fg = 211  # Mocha Red

        elif context.in_titlebar:
            attr |= bold
            if context.hostname:
                fg = 211 if context.bad else 151  # Mocha Red / Mocha Green
            elif context.directory:
                fg = 111  # Mocha Blue
            elif context.tab:
                if context.good:
                    bg = 151  # Mocha Green
            elif context.link:
                fg = 151  # Mocha Green

        elif context.in_statusbar:
            if context.permissions:
                if context.good:
                    fg = 151  # Mocha Green
                elif context.bad:
                    fg = 211  # Mocha Red
            if context.marked:
                attr |= bold | reverse
                fg = 111  # Mocha Blue
            if context.message:
                if context.bad:
                    attr |= bold
                    fg = 211  # Mocha Red
            if context.loaded:
                bg = self.progress_bar_color
            if context.vcsinfo:
                fg = 151  # Mocha Green
                attr &= ~bold
            if context.vcscommit:
                fg = 229  # Mocha Yellow
                attr &= ~bold

        if context.text:
            if context.highlight:
                attr |= reverse

        if context.in_taskview:
            if context.title:
                fg = 151  # Mocha Green

            if context.selected:
                attr |= reverse

            if context.loaded:
                if context.selected:
                    fg = self.progress_bar_color
                else:
                    bg = self.progress_bar_color

        if context.vcsfile and not context.selected:
            attr &= ~bold
            if context.vcsconflict:
                fg = 211  # Mocha Red
            elif context.vcschanged:
                fg = 211  # Mocha Red
            elif context.vcsunknown:
                fg = 211  # Mocha Red
            elif context.vcsstaged:
                fg = 151  # Mocha Green
            elif context.vcssync:
                fg = 151  # Mocha Green
            elif context.vcsignored:
                fg = default

        elif context.vcsremote and not context.selected:
            attr &= ~bold
            if context.vcssync:
                fg = 151  # Mocha Green
            elif context.vcsbehind:
                fg = 211  # Mocha Red
            elif context.vcsahead:
                fg = 151  # Mocha Green
            elif context.vcsdiverged:
                fg = 211  # Mocha Red
            elif context.vcsunknown:
                fg = 211  # Mocha Red

        return fg, bg, attr
