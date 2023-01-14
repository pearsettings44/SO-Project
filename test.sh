#! /bin/bash

for i in {1..15}; do
  echo $i
  sh -c "./subscriber/sub req ler$i box &"
done
