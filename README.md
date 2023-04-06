# simpsh

A simple shell written in C for University of Kentucky CS270, Systems Programming. Not fully funcional, a non-exhaustive list of incomplete functions:
- Redirection (Set up to handle 1 redirection, or the ./foo < in.txt > out.txt case)
- ENV (PATH, SHELL, and prev function return status are implemented)
- Signal Handling (only handles SIGINT)

Also includes a few helper functions. myecho.c prints args and env input back to stdout. wait.c waits for 10s (to test SIGINT handling)
## More to come later
