//+
Point(1) = {-1, -1, 0, 0.1};
//+
Point(2) = {1, -1, 0, 0.1};
//+
Point(3) = {1, 1, 0, 0.1};
//+
Point(4) = {-1, 1, 0, 0.1};
//+
Line(1) = {4, 3};
//+
Line(2) = {3, 2};
//+
Line(3) = {2, 1};
//+
Line(4) = {1, 4};
//+
Line Loop(1) = {4, 1, 2, 3};
//+
Plane Surface(1) = {1};