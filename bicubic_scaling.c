// https://www.paulinternet.nl/?page=bicubic

float interpolate_cubic(float p0, float p1, float p2, float p3, float x) {
	float a = -0.5 * p0 + 1.5 * p1 - 1.5 * p2 + 0.5 * p3;
	float b = p0 - 2.5 * p1 + 2 * p2 - 0.5 * p3;
	float c = -0.5 * p0 + 0.5 * p2;
	float d = p1;

	return a * x*x*x + b * x*x + c * x + d;
}


// This is too slow, it can be optimized in multilple ways, but maybe just use a library (with Lanczos interpolation) because it doesn't look that good at small sizes
char *bicubic_scaling(unsigned char *old_image, int ox, int oy, int pitch, float scale) {
	setbuf(stdout, NULL);
	int x = (float) ox * scale;
	int y = (float) oy * scale;
	printf("old: %d, %d, %d\n", ox, oy, pitch);
	printf("new: %d, %d\n", x, y);
	unsigned char *buf = malloc(4 * x * y * sizeof(*buf));

	for (int i = 0; i < y; ++i) {
		float oi = (i + 0.5) / scale - 0.5; // or add -+ 0.5 for some reason?
		for (int j = 0; j < x; ++j) {
			float oj = (j + 0.5) / scale - 0.5;

			int ioi = (int) oi;
			int ioj = (int) oj;

			float h[16] = {};
			for (int k = 0; k < 4; k++) {
				float v[16] = {};
				for (int f = 0; f < 16; f++) {
					if ((ioi + k - 1) < 0 || (ioj + f/4 - 1) < 0 || (ioi + k - 1) >= oy ||  (ioj + f/4 - 1) >= pitch) {
						break;
					}
					int idx = 4 * ((ioi + k - 1) * pitch + ioj - 1);
					if ((f % 4) == 3) {
						v[f] = old_image[idx + f/4 + 3] / 255.;
					} else {
						v[f] = pow((float) old_image[idx + f] / 255, 2.2);// * (old_image[idx + f/4 + 3] / 255.);  // multiplying be the alpha causes the image to be too dark?
					}
				}
				for (int q = 0; q < 3; ++q) {
					float r = interpolate_cubic(v[q + 0], v[q + 4], v[q + 8], v[q + 12], oj - ioj + 1); // Some implementations don't use +1? Why? doesn't seem to make a difference
					h[4 * k + q] = r;
				}

			}

			buf[4 * (i * x + j) + 3] = old_image[4 * (ioi * pitch + ioj) + 3]; // ??
			for (int q = 0; q < 3; ++q) {
				float r = interpolate_cubic(h[0 + q], h[4 + q], h[8 + q], h[12 + q], oi - ioi + 1);
				unsigned int z = (unsigned int) (pow(r, 1/2.2) * 255);
				if (z > 255) {
					z = 255;
					/* printf("X > 255\n"); */
				}
				buf[4*(i * x + j) + q] = z;// (unsigned  char) (pow(r, 1/2.2) * 255);
			}
		}
	}
	return buf;
}

char *area_averaging_scale(unsigned char *old_image, int ox, int oy, int pitch, float scale) {
	int x = (float) ox * scale;
	int y = (float) oy * scale;
	unsigned char *buf = malloc(4 * x * y * sizeof(*buf));
	for (int i = 0; i < y; ++i) {
		float oi = i / scale;
		/* float oi = (i + 0.5) / scale - 0.5; // or add -+ 0.5 for some reason? */
		for (int j = 0; j < x; ++j) {
			float oj = j / scale;
			/* float oj = (j + 0.5) / scale - 0.5; */

			int ioi = (int) oi;
			int ioj = (int) oj;

			int idx[] = {
				ioi, ioj,
				ioi + 1, ioj,
				ioi, ioj + 1,
				ioi + 1, ioj + 1
			};

			float color[4] = {};
			for (int z  = 0; z < 4; ++z) {
				int ycoord = idx[z * 2];
				int xcoord = idx[z * 2 + 1];
				if (ycoord >= oy || xcoord >= ox) continue;
				int idx = 4 * (ycoord * pitch + xcoord);

				color[0] += pow(old_image[idx] / 255., 2.2);
				color[1] += pow(old_image[idx + 1] / 255., 2.2);
				color[2] += pow(old_image[idx + 2] / 255., 2.2);
				color[3] += pow(old_image[idx + 3] / 255., 2.2);
			}

			for (int z = 0; z < 4; ++z) {
				unsigned int chlo = pow(color[z] / 4, 1/2.2) * 255;
				if (chlo > 255) {
					chlo = 255;
				}
				buf[4*(i * x + j) + z] = chlo;
			}
			/* buf[4*(i * x + j) + 3] = 255; */
		}
	}
	return buf;
}
