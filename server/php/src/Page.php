<?php

declare(strict_types=1);

namespace Tvshow;

/**
 * Full HTML document builder for tvshow pages.
 *
 * Usage:
 *   $page = new Page('Title', Page::THEME_TVISION);
 *   $page->body(Html::h1('Hello'), Html::p('World'));
 *   echo $page->render();
 */
class Page
{
    public const THEME_TVISION = 'tvision';
    public const THEME_DARK    = 'dark';
    public const THEME_LIGHT   = 'light';

    private string $title;
    private ?string $theme;
    private string $styles_base;
    /** @var string[] */
    private array $extra_links = [];
    /** @var string[] */
    private array $body_parts  = [];

    /**
     * @param string      $title       <title> text
     * @param string|null $theme       One of the THEME_* constants, or null for no built-in theme
     * @param string      $styles_base Base URL for built-in theme stylesheets (default /styles/)
     */
    public function __construct(
        string $title,
        ?string $theme = null,
        string $styles_base = '/styles/'
    ) {
        $this->title       = $title;
        $this->theme       = $theme;
        $this->styles_base = rtrim($styles_base, '/') . '/';
    }

    /** Add an extra <link rel="stylesheet"> to the <head>. */
    public function addStylesheet(string $href): static
    {
        $this->extra_links[] = $href;
        return $this;
    }

    /**
     * Append content to the <body>.
     * Each argument is a string or array<string>.
     * @param string|string[] ...$parts
     */
    public function body(string|array ...$parts): static
    {
        foreach ($parts as $part) {
            $this->body_parts[] = is_array($part) ? implode('', $part) : $part;
        }
        return $this;
    }

    /** Render and return the complete HTML document. */
    public function render(): string
    {
        $head = $this->renderHead();
        $body = implode("\n", $this->body_parts);
        return "<!doctype html>\n<html>\n" . $head . "\n<body>\n" . $body . "\n</body>\n</html>\n";
    }

    private function renderHead(): string
    {
        $lines   = [];
        $lines[] = '<head>';
        $lines[] = '<meta charset="utf-8">';
        $lines[] = '<title>' . Html::e($this->title) . '</title>';

        if ($this->theme !== null) {
            $href    = Html::e($this->styles_base . $this->theme . '.css');
            $lines[] = '<link rel="stylesheet" href="' . $href . '">';
        }

        foreach ($this->extra_links as $href) {
            $lines[] = '<link rel="stylesheet" href="' . Html::e($href) . '">';
        }

        $lines[] = '</head>';
        return implode("\n", $lines);
    }
}
