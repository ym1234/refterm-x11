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
		float oi = i / scale; // or add -+ 0.5 for some reason?
		for (int j = 0; j < x; ++j) {
			float oj = j / scale;

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
					float r = interpolate_cubic(v[q + 0], v[q + 4], v[q + 8], v[q + 12], oj - ioj);
					h[4 * k + q] = r;
				}

			}

			buf[4 * (i * x + j) + 3] = old_image[4 * (ioi * pitch + ioj) + 3]; // ??
			for (int q = 0; q < 3; ++q) {
				float r = interpolate_cubic(h[0 + q], h[4 + q], h[8 + q], h[12 + q], oi - ioi);
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
