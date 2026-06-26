<?php

declare(strict_types=1);

namespace Tvshow;

/**
 * Static helpers that generate tvshow-compatible HTML fragments.
 *
 * All methods return strings. $content / $children accept a string or
 * array<string> (auto-joined without separator).
 */
class Html
{
    // ── primitives ────────────────────────────────────────────────────────────

    /** @param string|string[] $content */
    public static function div(string|array $content, string $class = '', string $style = ''): string
    {
        return self::tag('div', $content, $class, $style);
    }

    /** @param string|string[] $content */
    public static function p(string|array $content, string $class = ''): string
    {
        return self::tag('p', $content, $class);
    }

    /** @param string|string[] $content */
    public static function span(string|array $content, string $class = ''): string
    {
        return self::tag('span', $content, $class);
    }

    /** @param string|string[] $content */
    public static function h1(string|array $content): string { return self::tag('h1', $content); }
    /** @param string|string[] $content */
    public static function h2(string|array $content): string { return self::tag('h2', $content); }
    /** @param string|string[] $content */
    public static function h3(string|array $content): string { return self::tag('h3', $content); }
    /** @param string|string[] $content */
    public static function h4(string|array $content): string { return self::tag('h4', $content); }
    /** @param string|string[] $content */
    public static function h5(string|array $content): string { return self::tag('h5', $content); }
    /** @param string|string[] $content */
    public static function h6(string|array $content): string { return self::tag('h6', $content); }

    public static function a(string $href, string $label, string $class = ''): string
    {
        $c = $class !== '' ? ' class="' . self::e($class) . '"' : '';
        return '<a href="' . self::e($href) . '"' . $c . '>' . self::e($label) . '</a>';
    }

    /** @param string|string[] $content */
    public static function pre(string|array $content): string { return self::tag('pre', $content); }

    /** @param string|string[] $content */
    public static function code(string|array $content): string { return self::tag('code', $content); }

    /** @param string|string[] $content */
    public static function blockquote(string|array $content): string { return self::tag('blockquote', $content); }

    /** @param string|string[] $content */
    public static function strong(string|array $content): string { return self::tag('strong', $content); }

    /** @param string|string[] $content */
    public static function em(string|array $content): string { return self::tag('em', $content); }

    /** @param string|string[] $content */
    public static function small(string|array $content): string { return self::tag('small', $content); }

    /** @param string|string[] $content */
    public static function u(string|array $content): string { return self::tag('u', $content); }

    public static function br(): string { return '<br>'; }
    public static function hr(): string { return '<hr>'; }

    /** @param string[] $items */
    public static function ul(array $items, string $class = ''): string
    {
        $li = implode('', array_map(fn($i) => '<li>' . $i . '</li>', $items));
        return self::tag('ul', $li, $class);
    }

    /** @param string[] $items */
    public static function ol(array $items, string $class = ''): string
    {
        $li = implode('', array_map(fn($i) => '<li>' . $i . '</li>', $items));
        return self::tag('ol', $li, $class);
    }

    /**
     * Image placeholder.  width_px / height_px are rounded to the nearest
     * character cell (8px = 1col, 16px = 1row) so the reserved space matches
     * what tvshow will allocate.
     */
    public static function img(string $src, string $alt, ?int $width_px = null, ?int $height_px = null): string
    {
        $attrs = ' src="' . self::e($src) . '" alt="' . self::e($alt) . '"';
        if ($width_px !== null) {
            $attrs .= ' width="' . self::snapW($width_px) . '"';
        }
        if ($height_px !== null) {
            $attrs .= ' height="' . self::snapH($height_px) . '"';
        }
        return '<img' . $attrs . '>';
    }

    // ── theme-class shortcuts ─────────────────────────────────────────────────

    /**
     * Flex row. $gap is a valid CSS value (default 2ch).
     * @param string|string[] $children
     */
    public static function row(string|array $children, string $gap = '2ch'): string
    {
        $style = $gap !== '2ch' ? 'gap:' . self::e($gap) : '';
        return self::tag('div', $children, 'row', $style);
    }

    /**
     * Horizontal nav bar.
     * @param string[] $links — array of Html::a() strings
     */
    public static function nav(array $links): string
    {
        return self::tag('div', implode(' ', $links), 'nav');
    }

    /**
     * Card component (bordered container with title).
     * @param string|string[] $body
     */
    public static function card(string $title, string|array $body, string $extra_class = 'grow'): string
    {
        $classes = 'card' . ($extra_class !== '' ? ' ' . $extra_class : '');
        return self::tag('div',
            '<p class="card-title">' . self::e($title) . '</p>' . self::join($body),
            $classes
        );
    }

    /**
     * Panel component (heavier bordered container with title).
     * @param string|string[] $body
     */
    public static function panel(string $title, string|array $body): string
    {
        return self::tag('div',
            '<p class="panel-title">' . self::e($title) . '</p>' . self::join($body),
            'panel'
        );
    }

    /**
     * Alert box. $type: info|success|warning|error
     */
    public static function alert(string $message, string $type = 'info'): string
    {
        $allowed = ['info', 'success', 'warning', 'error'];
        $t = in_array($type, $allowed, true) ? $type : 'info';
        return self::tag('div', self::e($message), 'alert alert-' . $t);
    }

    /** Dim secondary text (.muted). */
    public static function muted(string $text): string
    {
        return self::tag('span', $text, 'muted');
    }

    /** Bold text (.bold). */
    public static function bold(string $text): string
    {
        return self::tag('span', $text, 'bold');
    }

    // ── internal helpers ──────────────────────────────────────────────────────

    /** @param string|string[] $content */
    private static function tag(string $tag, string|array $content, string $class = '', string $style = ''): string
    {
        $attrs = '';
        if ($class !== '') {
            $attrs .= ' class="' . self::e($class) . '"';
        }
        if ($style !== '') {
            $attrs .= ' style="' . self::e($style) . '"';
        }
        return '<' . $tag . $attrs . '>' . self::join($content) . '</' . $tag . '>';
    }

    /** @param string|string[] $v */
    private static function join(string|array $v): string
    {
        return is_array($v) ? implode('', $v) : $v;
    }

    /** HTML-escape a value for use in attribute or text context. */
    public static function e(string $v): string
    {
        return htmlspecialchars($v, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
    }

    /** Round px to nearest 8-px column boundary. */
    private static function snapW(int $px): int
    {
        return (int) round($px / 8) * 8;
    }

    /** Round px to nearest 16-px row boundary. */
    private static function snapH(int $px): int
    {
        return (int) round($px / 16) * 16;
    }
}
