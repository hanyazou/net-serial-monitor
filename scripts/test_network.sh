#!/bin/bash

// -n: numeric, -c 1: one packet, -w 1 deadline 1s, -W 1 per-reply timeout.
ping -n -c 1 -w 1 -W 1 192.168.0.1
