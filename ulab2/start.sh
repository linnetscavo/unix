#!/bin/bash

SHARED_DIR="/app/shared_volume"
LOCK_FILE="${SHARED_DIR}/sync.lock"

if [ ! -d "$SHARED_DIR" ]; then
    echo "Ошибка: Каталог общего тома ${SHARED_DIR} не найден."
    exit 1
fi

CONTAINER_ID=$(tr -dc A-Za-z0-9 </dev/urandom | head -c 8)
FILE_COUNT=0

echo "Контейнер запущен с ID: ${CONTAINER_ID}"
echo "Лог: ID, Номер файла, Имя файла, Операция, Время"
echo "----------------------------------------------------"

while true; do
    TARGET_FILENAME=""
    (
        flock -x 9 

        for i in $(seq -w 1 999); do
            FILENAME="${SHARED_DIR}/${i}.txt"
            if [ ! -f "$FILENAME" ]; then
                echo "$CONTAINER_ID:$((FILE_COUNT + 1))" > "$FILENAME"
                FILE_COUNT=$((FILE_COUNT + 1))
                TARGET_FILENAME="$FILENAME"
                echo "${CONTAINER_ID},${FILE_COUNT},$(basename "$FILENAME"),CREATED,$(date +%s.%N)"
                break
            fi
        done
    ) 9>"$LOCK_FILE"

    if [ -n "$TARGET_FILENAME" ]; then
        sleep 1
        rm -f "$TARGET_FILENAME"
        echo "${CONTAINER_ID},${FILE_COUNT},$(basename "$TARGET_FILENAME"),DELETED,$(date +%s.%N)"
    fi

    sleep 1

done