#! /bin/bash

for i in {1..8}; do
  echo $i
  sh -c "./publisher/pub req wr$i a &"
done
