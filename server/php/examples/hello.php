<?php

declare(strict_types=1);

require_once __DIR__ . '/../src/Html.php';
require_once __DIR__ . '/../src/Form.php';
require_once __DIR__ . '/../src/Page.php';

use Tvshow\Html;
use Tvshow\Page;

$page = new Page('Hello, tvshow!', Page::THEME_DARK);
$page->body(
    Html::h1('Hello, tvshow!'),
    Html::p('A minimal page rendered in the terminal.'),
    Html::p(
        'This is ' . Html::strong('bold') . ', this is ' . Html::em('italic') . ', '
        . 'and this is a ' . Html::a('/other', 'link') . '.'
    ),
    Html::hr(),
    Html::muted('Powered by tvshow.')
);

echo $page->render();
