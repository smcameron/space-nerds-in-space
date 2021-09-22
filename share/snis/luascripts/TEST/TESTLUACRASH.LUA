-- do some stupid stuff to make the lua program crash

function quotient(a, b)
	demon_print("a / b is " .. a / b);
end

function inner()
	quotient(1, 2);
	quotient(1, 3);
	quotient(1, nil); -- intentional use of nil to cause crash
end

function middle()
	inner();
end

function outer()
	middle();
end

outer();

