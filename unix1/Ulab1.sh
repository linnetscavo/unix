#!/bin/sh

cleanup() {
    rc=$?
    if [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
        rm -rf "$TMP_DIR"
    fi
    exit $rc
}

if [ $# -ne 1 ]; then
    echo "Использование: $0 <файл.cpp>" >&2
    exit 1
fi

SRC="$1"

if [ ! -e "$SRC" ]; then
    echo "Ошибка: файл '$SRC' не существует." >&2
    exit 2
fi
if [ ! -r "$SRC" ]; then
    echo "Ошибка: файл '$SRC' недоступен для чтения." >&2
    exit 3
fi

OUTPUT=$(grep -m 1 '^// &Output:' "$SRC" | sed 's|^// &Output:[[:space:]]*||')

if [ -z "$OUTPUT" ]; then
    echo "Ошибка: не найден комментарий '// &Output:'." >&2
    exit 4
fi

TMP_DIR=$(mktemp -d) || {
    echo "Ошибка: не удалось создать временный каталог." >&2
    exit 5
}

trap cleanup EXIT HUP INT QUIT TERM

cp "$SRC" "$TMP_DIR/"
cd "$TMP_DIR"

SRC_NAME=$(basename "$SRC")

if g++ -o "$OUTPUT" "$SRC_NAME"; then
    cp "$OUTPUT" "../$OUTPUT"
else
    echo "Ошибка: компиляция не удалась." >&2
    exit 6
fi

echo "Успех: результат — '../$OUTPUT'"