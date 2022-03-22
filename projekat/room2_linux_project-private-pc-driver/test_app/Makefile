all: test_app
 
# $@ matches the target; $< matches the first dependent
test_app: test_app.o
	${CROSS_COMPILE}gcc -o $@ $< -static

test_app.o: test_app.c
	${CROSS_COMPILE}gcc -c $< -static
    
clean:
	rm test_app.o test_app