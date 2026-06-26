<?php

declare(strict_types=1);

require_once __DIR__ . '/../src/Html.php';
require_once __DIR__ . '/../src/Form.php';
require_once __DIR__ . '/../src/Page.php';

use Tvshow\Html;
use Tvshow\Page;

$page = new Page('Dashboard', Page::THEME_TVISION);
$page->body(
    Html::nav([
        Html::a('/', 'Home'),
        Html::a('/status', 'Status'),
        Html::a('/config', 'Config'),
    ]),

    Html::h1('Dashboard'),

    Html::alert('All systems operational.', 'success'),

    Html::row([
        Html::card('CPU', Html::p('42%')),
        Html::card('Memory', Html::p('2.1 G / 8 G')),
        Html::card('Uptime', Html::p('14 days')),
    ]),

    Html::h2('Recent events'),

    Html::panel('Event log', implode('', [
        Html::p(Html::muted('2026-06-26 08:01') . ' — Service started'),
        Html::p(Html::muted('2026-06-26 07:44') . ' — Config reloaded'),
        Html::p(Html::muted('2026-06-25 23:00') . ' — ' . Html::bold('Backup completed')),
    ])),

    Html::p(Html::a('/events', 'View all events'))
);

echo $page->render();
