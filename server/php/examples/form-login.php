<?php

declare(strict_types=1);

require_once __DIR__ . '/../src/Html.php';
require_once __DIR__ . '/../src/Form.php';
require_once __DIR__ . '/../src/Page.php';

use Tvshow\Html;
use Tvshow\Form;
use Tvshow\Page;

// Simulate a POST handler: if username posted, show a success message.
$posted_user = $_POST['username'] ?? null;
$error       = ($posted_user !== null && $posted_user === '') ? 'Username is required.' : null;
$success     = ($posted_user !== null && $posted_user !== '');

$page = new Page('Login', Page::THEME_LIGHT);
$page->body(
    Html::nav([Html::a('/', 'Home')]),
    Html::h1('Login'),

    $success
        ? Html::alert('Welcome, ' . Html::e($posted_user) . '!', 'success')
        : '',

    $error !== null
        ? Html::alert($error, 'error')
        : '',

    !$success
        ? implode('', [
            Form::open('/login', 'post'),
            Form::group('Username', Form::text('username', '', 'user@example.com', 'username'), 'username'),
            Form::group('Password', Form::password('password', '', 'password'), 'password'),
            Html::div(
                implode(' ', [
                    Form::submit('Login', 'btn btn-primary'),
                    Html::a('/reset', 'Forgot password?', 'muted'),
                ]),
                'form-group'
            ),
            Form::close(),
        ])
        : Html::p(Html::a('/', 'Go to dashboard')),
);

echo $page->render();
