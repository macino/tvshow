<?php

declare(strict_types=1);

namespace Tvshow;

/**
 * Static helpers for tvshow-compatible HTML forms.
 *
 * Constraints enforced:
 *  - enctype is never set (application/x-www-form-urlencoded only; no multipart)
 *  - method is normalised to lowercase "get" or "post"
 */
class Form
{
    // ── form container ────────────────────────────────────────────────────────

    public static function open(string $action, string $method = 'post'): string
    {
        $m = strtolower($method) === 'get' ? 'get' : 'post';
        return '<form action="' . Html::e($action) . '" method="' . $m . '">';
    }

    public static function close(): string
    {
        return '</form>';
    }

    // ── field wrapper ─────────────────────────────────────────────────────────

    /**
     * Wraps a label + control in a .form-group container.
     * $label_for should match the control's id attribute when set.
     */
    public static function group(string $label, string $control, string $label_for = ''): string
    {
        $for = $label_for !== '' ? ' for="' . Html::e($label_for) . '"' : '';
        $lbl = '<label class="form-label"' . $for . '>' . Html::e($label) . '</label>';
        return '<div class="form-group">' . $lbl . $control . '</div>';
    }

    // ── controls ──────────────────────────────────────────────────────────────

    public static function text(
        string $name,
        string $value = '',
        string $placeholder = '',
        string $id = ''
    ): string {
        return self::input('text', $name, $value, $placeholder, $id);
    }

    public static function password(
        string $name,
        string $value = '',
        string $id = ''
    ): string {
        return self::input('password', $name, $value, '', $id);
    }

    public static function textarea(
        string $name,
        int $rows = 3,
        string $value = '',
        string $id = ''
    ): string {
        $id_attr = $id !== '' ? ' id="' . Html::e($id) . '"' : '';
        return '<textarea name="' . Html::e($name) . '"'
            . $id_attr
            . ' rows="' . $rows . '">'
            . Html::e($value)
            . '</textarea>';
    }

    public static function checkbox(
        string $name,
        string $value = '1',
        bool $checked = false,
        string $label = '',
        string $id = ''
    ): string {
        $id_attr = $id !== '' ? ' id="' . Html::e($id) . '"' : '';
        $chk = $checked ? ' checked' : '';
        $input = '<input type="checkbox" name="' . Html::e($name) . '"'
            . ' value="' . Html::e($value) . '"'
            . $id_attr . $chk . '>';
        if ($label !== '') {
            $for = $id !== '' ? ' for="' . Html::e($id) . '"' : '';
            $input .= '<label' . $for . '> ' . Html::e($label) . '</label>';
        }
        return $input;
    }

    public static function radio(
        string $name,
        string $value,
        bool $checked = false,
        string $label = '',
        string $id = ''
    ): string {
        $id_attr = $id !== '' ? ' id="' . Html::e($id) . '"' : '';
        $chk = $checked ? ' checked' : '';
        $input = '<input type="radio" name="' . Html::e($name) . '"'
            . ' value="' . Html::e($value) . '"'
            . $id_attr . $chk . '>';
        if ($label !== '') {
            $for = $id !== '' ? ' for="' . Html::e($id) . '"' : '';
            $input .= '<label' . $for . '> ' . Html::e($label) . '</label>';
        }
        return $input;
    }

    /**
     * @param array<string,string> $options  value => display-label pairs
     */
    public static function select(
        string $name,
        array $options,
        string $selected = '',
        string $id = ''
    ): string {
        $id_attr = $id !== '' ? ' id="' . Html::e($id) . '"' : '';
        $opts = '';
        foreach ($options as $val => $label) {
            $sel = (string) $val === $selected ? ' selected' : '';
            $opts .= '<option value="' . Html::e((string) $val) . '"' . $sel . '>'
                . Html::e($label)
                . '</option>';
        }
        return '<select name="' . Html::e($name) . '"' . $id_attr . '>' . $opts . '</select>';
    }

    public static function hidden(string $name, string $value): string
    {
        return '<input type="hidden" name="' . Html::e($name) . '" value="' . Html::e($value) . '">';
    }

    public static function submit(string $label = 'Submit', string $class = 'btn btn-primary'): string
    {
        return '<input type="submit" value="' . Html::e($label) . '" class="' . Html::e($class) . '">';
    }

    public static function button(string $label, string $class = 'btn'): string
    {
        return '<button type="submit" class="' . Html::e($class) . '">' . Html::e($label) . '</button>';
    }

    // ── internal ──────────────────────────────────────────────────────────────

    private static function input(
        string $type,
        string $name,
        string $value = '',
        string $placeholder = '',
        string $id = ''
    ): string {
        $id_attr = $id !== '' ? ' id="' . Html::e($id) . '"' : '';
        $ph = $placeholder !== '' ? ' placeholder="' . Html::e($placeholder) . '"' : '';
        return '<input type="' . $type . '"'
            . ' name="' . Html::e($name) . '"'
            . ' value="' . Html::e($value) . '"'
            . $id_attr . $ph . '>';
    }
}
