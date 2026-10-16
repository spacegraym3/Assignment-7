#!/bin/bash
binary="client server"

make clean; make

score=0

# Every directory under reference/ (t1, t2, ...) is one test case.
# Each reference clientN.out is worth 1 point, plus 1 for questions.txt.
shopt -s nullglob
tests=(reference/t*/)
all_refs=(reference/t*/client*.out)
total=$(( ${#all_refs[@]} + 1 ))

for file in $binary; do
    if [[ ! -f "$file" ]]; then
        echo "FAIL: $file not made"
        echo "SCORE: $score/$total"
        exit 0
    fi
done

# Start the server, run every clientN.in in $dir as a client, then kill the server.
run_clients() {
    rm -f client*.out

    ./server > /dev/null 2>&1 &
    server_pid=$!
    sleep 1   # give the server time to start listening

    # timeout prevents a hung client from stalling the grader
    client_pids=()
    for input in "$dir"/client*.in; do
        name=$(basename "$input" .in)   # e.g. client3
        cat "$input" | timeout 10 ./client > "$name.out" &
        client_pids+=($!)
    done

    # Wait for all clients to finish, then stop the server
    for pid in "${client_pids[@]}"; do
        wait "$pid"
    done
    kill $server_pid 2>/dev/null
    wait $server_pid 2>/dev/null   # reap it and suppress the "Terminated" message
}

# Crash test: each client is ONE process. It gets s1-clientN.in, then the server
# is killed and restarted with ./server 1, then the same client gets s2-clientN.in.
run_crash() {
    rm -f client*.out .restarted

    ./server > /dev/null 2>&1 &
    server_pid=$!
    sleep 1

    client_pids=()
    for input in "$dir"/s1-client*.in; do
        name=$(basename "$input" .in)
        name=${name#s1-}
        # Feed s1, hold stdin open until the server has restarted, then feed s2
        {
            cat "$input"
            while [ ! -f .restarted ]; do sleep 0.1; done
            cat "$dir/s2-$name.in" 2>/dev/null
        } | timeout 20 ./client > "$name.out" &
        client_pids+=($!)
    done

    sleep 1   # let the clients process their s1 input

    kill $server_pid 2>/dev/null
    wait $server_pid 2>/dev/null
    ./server 1 > /dev/null 2>&1 &
    server_pid=$!
    sleep 1
    touch .restarted   # release the s2 input to every client

    for pid in "${client_pids[@]}"; do
        wait "$pid"
    done
    kill $server_pid 2>/dev/null
    wait $server_pid 2>/dev/null
    rm -f .restarted
}

pkill server
pkill client

for dir in "${tests[@]}"; do
    dir=${dir%/}
    echo "TEST: $(basename "$dir")"

    if [[ $dir == *crash* ]]; then
        run_crash
    else
        run_clients
    fi

    # Grade against whichever clientN.out files the reference provides
    for ref in "$dir"/client*.out; do
        name=$(basename "$ref" .out)
        if diff -q "$ref" "$name.out" > /dev/null 2>&1; then
            ((score+=1))
        else
            echo "  --FAIL! $name.out does not match $ref"
        fi
    done
done

echo "TEST: questions.txt is non-empty and contains questions (1) through (N)"
N=2
if [ -f questions.txt ] && [ "$(wc -c < questions.txt)" -gt 200 ]; then
    passed=true
    for ((i=1; i<=N; i++)); do
        if ! grep -q "^($i)" questions.txt; then
            echo "  --FAIL: questions.txt missing ($i)"
            passed=false
        fi
    done
    if $passed; then
        ((score+=1))
    fi
else
    echo "  --FAIL!"
fi

echo "SCORE: $score/$total"