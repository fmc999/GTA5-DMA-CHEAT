#pragma once

struct Vec3
{
	float x, y, z;

public:
	float Distance(Vec3 Destination)
	{

		float x = (Destination.x - this->x);
		x = x * x;

		float y = (Destination.y - this->y);
		y = y * y;

		float z = (Destination.z - this->z);
		z = z * z;

		float Intermediate = x + y + z;
	
		float Return = sqrt(Intermediate);

		return Return;
	}
};