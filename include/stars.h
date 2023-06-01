typedef struct star_t { 
	// Bayer name for the star.
	char *name_bayer;

	// Proper name of the star.  Usually a weird arabic name. :)
	char *name_proper;

	// Spectral classification of the main stars in the system.
	char *spec_type;

	// Right Ascension in hours and minutes for epoch 2000.
	double ra;

	// Declination in degrees for epoch 2000.
	double dec;

	// Galactic longitude of the star.
	double gal_lat;

	// Galactic latitude of the star.
	double gal_long;

	// Visual magnitude of the star.
	double mag_vis;

	// Absolute magnitude of the star.
	double mag_abs;

	// The Hipparcos parallax of the star (x1000).
	double hip_parlx;

	// The error in the parallax (x1000).
	double hip_parlx_err;

	// The distance in light years (=3.2616/parallax).
	double dist_ly;

} star_t;

extern const star_t stars[];
int _num_stars();

#define NUM_STARS _num_stars()

