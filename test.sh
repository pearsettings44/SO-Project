#! /bin/bash

for i in {1..8}; do
  echo $i
  sh -c "./subscriber/sub req ler$i a &"
done

