/* TODO: Massive cleanup */
#include <stdio.h>
#include <stdlib.h>
#include <SDL/SDL.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
/* screen size stuff */
#define WIDTH  800
#define HEIGHT 600
#define MAX_DISTANCE 1000

#define NUM_BIRDS 10
#define NUM_BEES 200

#define ORG_TYPE_BEE  1
#define ORG_TYPE_BIRD 2

/** configuration **/

#define AIR_RESIST 0.1 /* how much an organism is slowed down by the 'air' */

#define MAX 20         /* maximum value for a gene */
#define MAX_SPEED 8    /* maximum value for the speed gene */
#define MAX_MUT (MAX*2)/* maximum value a mutation can have */

#define HEALTH 400     /* starting health of a bird */

#define HEAL 8         /* how much a bird is healed when it eats a bee */

#define DELAY 16       /* how many milliseconds to pause between each tick */

#define GAME_LENGTH 10 /* how many seconds per generation */

#define MUTATION_RATE 0.03 /* how likely it is that an organism will have a mutation */

/* these control the result of the fitness calculation function */
#define KILLS_REWARD 10
#define FOOD_REWARD 1

SDL_Surface *screen;

struct genes {
	float speed;
	float accel;
	float attack;
	float determination;
	float frightfulness;
	float aggressiveness;
	float friendliness;
	float claustrophobia;
};

struct organism {
	struct genes genetics;
	int dead;
	unsigned health;
	float x, y, start_x, start_y;
	float vel_x, vel_y;
	float accel_x, accel_y;
	int type;
	int goal_x, goal_y;
	int brought_food, has_food;
	int near_enemy;
	double near_enemy_dist, neighbor_dist;
	int neighbor;
	int kills;
	int fitness;
	int moved;
};

struct organism creatures[NUM_BEES + NUM_BIRDS];

int hive_x, hive_y, food_x, food_y, center_x, center_y;

void putpixels(int x, int y, int color)
{
	unsigned int *ptr = (unsigned int*)screen->pixels;
	int lineoffset = y * (screen->pitch / sizeof( unsigned int ) );
	ptr[lineoffset + x ] = color;
}

double distance(float x1, float y1, float x2, float y2)
{
	x1 -= x2;
	y1 -= y2;
	return sqrt(x1*x1 + y1*y1);
}

/* changes the accel_ values to where the organism wants to go */
void select_desired_direction(struct organism *o, int ignore)
{
	unsigned score_det=0, score_fright=0, score_agg=0, score_friend=0, score_cla=0;
	unsigned u[5];
	
	u[0] = score_det    = (MAX_DISTANCE-distance(o->x, o->y, o->goal_x, o->goal_y)) * o->genetics.determination;
	u[1] = score_fright = (MAX_DISTANCE-o->near_enemy_dist) * o->genetics.frightfulness;
	u[2] = score_agg    = (MAX_DISTANCE-o->near_enemy_dist) * o->genetics.aggressiveness;
	u[3] = score_friend = (MAX_DISTANCE-o->neighbor_dist)  * o->genetics.friendliness;
	u[4] = score_cla    = (MAX_DISTANCE-o->neighbor_dist)  * o->genetics.claustrophobia;
	
	if(o->neighbor_dist > (3 + (o->type == ORG_TYPE_BIRD ? 5 : 0)))
		u[4] = score_cla = 0;
	
	if(o->type == ORG_TYPE_BIRD) {
		u[1] = u[0] = u[3] = 0;
		//u[2] = score_agg    = (MAX_DISTANCE-distance(o->x, o->y, center_x, center_y)) * o->genetics.aggressiveness;
	}
	
	if(o->near_enemy_dist < 3) {
		u[2] = score_agg = score_agg * (MAX_DISTANCE-o->near_enemy_dist);
	}
	
	float target_x, target_y;
	int run_away=0;
	
	int i;
	int big=-1;
	unsigned largest=0;
	for(i=0;i<5;i++)
	{
		if((big == -1 || u[i] > largest) && i != ignore) {
			largest = u[i];
			big = i;
		}
	}
	
	if(big == 1 || big == 4)
		run_away = 1;

	switch(big) {
		case 0:
			target_x = o->goal_x;
			target_y = o->goal_y;
			break;
		case 1:
			target_x = creatures[o->near_enemy].x;
			target_y = creatures[o->near_enemy].y;
			break;
		case 2:
			target_x = creatures[o->near_enemy].x;
			target_y = creatures[o->near_enemy].y;
			break;
		case 3:
			target_x = creatures[o->neighbor].x;
			target_y = creatures[o->neighbor].y;
			break;
		case 4:
			target_x = creatures[o->neighbor].x;
			target_y = creatures[o->neighbor].y;
			break;
	}
	
	if(o->type == ORG_TYPE_BEE && (o->x == 0 || o->x == WIDTH-1 || o->y == 0 || o->y == HEIGHT-1)) {
		target_x = hive_x;
		target_y = hive_y;
	}
	
	float dir_x = target_x - o->x;
	float dir_y = target_y - o->y;
	if(run_away) {
		dir_x = -dir_x;
		dir_y = -dir_y;
	}
	
	float mag = sqrt(dir_x*dir_x + dir_y*dir_y);
	dir_x /= mag;
	dir_y /= mag;
	o->accel_x = dir_x * o->genetics.accel;
	o->accel_y = dir_y * o->genetics.accel;
}

void check_collisions(struct organism *o)
{
	int i;
	double shortest=INFINITY;
	double shortest_bird=INFINITY;
	
	for(i=0;i<NUM_BEES + NUM_BIRDS;i++)
	{
		if(o != &creatures[i] && !creatures[i].dead) {
			struct organism *b = &creatures[i];
			if(b->type == o->type)
			{
				float _x, _y;
				_x = b->x - o->x;
				_y = b->y - o->y;
				if(sqrt(_x*_x + _y*_y) < shortest)
				{
					shortest = sqrt(_x*_x + _y*_y);
					o->neighbor = i;
					o->neighbor_dist = shortest;
				}
			}
			if(b->type != o->type)
			{
				float _x, _y;
				_x = b->x - o->x;
				_y = b->y - o->y;
				if(sqrt(_x*_x + _y*_y) < shortest_bird)
				{
					shortest_bird = sqrt(_x*_x + _y*_y);
					o->near_enemy = i;
					o->near_enemy_dist = shortest_bird;
				}
			}
			
			if(o->type != b->type && o->type == ORG_TYPE_BEE) {
				if((o->x - b->x) < 5 && (o->x - b->x) >= 0 && (o->y - b->y) < 5 && (o->y - b->y) >= 0)
				{
					if(fabs(b->vel_x) > fabs(b->vel_y))
					{
						if(b->vel_x > 0)
						{
							/* -> */
							if((int)o->x == (int)b->x + 4)
							{
								/* bee is eaten */
								o->dead = 1;
								b->kills++;
								b->health+=(1+o->has_food) * HEAL;
							} else if(!o->has_food) {
								o->kills++;
								b->health -= o->genetics.attack;
								if(b->health <= 0) b->dead = 1;
							}
						}
						else
						{
							/* <- */
							if((int)o->x == (int)b->x)
							{
								/* bee is eaten */
								o->dead = 1;
								b->kills++;
								b->health+=(1+o->has_food) * HEAL;
							} else if(!o->has_food) {
								o->kills++;
								b->health -= o->genetics.attack;
								if(b->health <= 0) b->dead = 1;
							}
						}
					}
					else// if(fabs(b->vel_y) > fabs(b->vel_x))
					{
						if(b->vel_y > 0)
						{
							/* ^ */
							if((int)o->y == (int)b->y)
							{
								/* bee is eaten */
								o->dead = 1;
								b->kills++;
								b->health+=(1+o->has_food) * HEAL;
							} else if(!o->has_food) {
								o->kills++;
								b->health -= o->genetics.attack;
								if(b->health <= 0) b->dead = 1;
							}
						}
						else
						{
							/* down */
							if((int)o->y == (int)b->y+4)
							{
								/* bee is eaten */
								o->dead = 1;
								b->kills++;
								b->health+=(1+o->has_food) * HEAL;
							} else if(!o->has_food) {
								o->kills++;
								b->health -= o->genetics.attack;
								if(b->health <= 0) b->dead = 1;
							}
						}
					}
				}
			}
		}
	}
	
	if(o->type == ORG_TYPE_BEE)
	{
		if(distance(o->x, o->y, food_x, food_y) < 5)
		{
			o->goal_x = hive_x;
			o->goal_y = hive_y;
			o->has_food = 1;
		}
		if(distance(o->x, o->y, hive_x, hive_y) < 5)
		{
			o->goal_x = food_x;
			o->goal_y = food_y;
			if(o->has_food)
				o->brought_food++;
			o->has_food = 0;
		}
	}
	
}

void move_organism(struct organism *o)
{
	if(o->vel_x > 0)
	{
		if(o->vel_x > AIR_RESIST)
			o->vel_x -= AIR_RESIST;
		else
			o->vel_x=0;
	}
	if(o->vel_x < 0)
	{
		if(fabs(o->vel_x) > AIR_RESIST)
			o->vel_x += AIR_RESIST;
		else
			o->vel_x=0;
	}
	
	if(o->vel_y > 0)
	{
		if(o->vel_y > AIR_RESIST)
			o->vel_y -= AIR_RESIST;
		else
			o->vel_y=0;
	}
	if(o->vel_y < 0)
	{
		if(fabs(o->vel_y) > AIR_RESIST)
			o->vel_y += AIR_RESIST;
		else
			o->vel_y=0;
	}
	
	float new_vx = o->vel_x + o->accel_x, new_vy = o->vel_y + o->accel_y;
	if(sqrt(new_vx * new_vx + new_vy * new_vy) < o->genetics.speed) {
		o->vel_x = new_vx;
		o->vel_y = new_vy;
	}
	
	float newx = o->x + o->vel_x;
	float newy = o->y + o->vel_y;
	if(newx < 0)
		newx=o->vel_x=0;
	if(newy < 0)
		newy=o->vel_y=0;
	if(newx + (o->type == ORG_TYPE_BIRD ? 5 : 0) >= WIDTH) {
		newx = WIDTH - (o->type == ORG_TYPE_BIRD ? 6 : 1);
		o->vel_x=0;
	}
	if(newy + (o->type == ORG_TYPE_BIRD ? 5 : 0) >= HEIGHT) {
		newy = HEIGHT - (o->type == ORG_TYPE_BIRD ? 6 : 1);
		o->vel_y=0;
	}
	if(newx != o->x || newy != o->y)
		o->moved=1;
	o->x = newx;
	o->y = newy;
}

void do_update(struct organism *o)
{
	if(!o->dead)
		check_collisions(o);
	if(!o->dead)
		select_desired_direction(o, -1);
	SDL_Rect r;
	r.h = 5;
	r.w = 5;
	if(o->type == ORG_TYPE_BEE)
		putpixels(o->x, o->y, SDL_MapRGB(screen->format, 0, 0, 0));
	else {
		r.x = (int)o->x;
		r.y = (int)o->y;
		SDL_FillRect(screen, &r, SDL_MapRGB(screen->format, 0, 0, 0));
	}
	if(!o->dead) {
		move_organism(o);
		if(o->type == ORG_TYPE_BEE)
			putpixels(o->x, o->y, SDL_MapRGB(screen->format, 255, 255, 255));
		else {
			//int nowhere;
			r.x = (int)o->x;
			r.y = (int)o->y;
			SDL_FillRect(screen, &r, SDL_MapRGB(screen->format, 255, 165, 0));
			if(fabs(o->vel_x) > fabs(o->vel_y))
			{
				r.h=5;
				r.w=1;
				if(o->vel_x > 0)
				{
					/* -> */
					r.x = o->x+4;
					r.y = o->y;
				}
				else
				{
					/* <- */
					r.x = o->x;
					r.y = o->y;
				}
			}
			else// if(fabs(o->vel_y) > fabs(o->vel_x))
			{
				r.h=1;
				r.w=5;
				if(o->vel_y > 0)
				{
					/* ^ */
					r.x = o->x;
					r.y = o->y;
				}
				else
				{
					/* down */
					r.x = o->x;
					r.y = o->y+4;
				}
			}
			SDL_FillRect(screen, &r, SDL_MapRGB(screen->format, 255, 0, 0));
		}
	}
}

void draw_objects()
{
	SDL_Rect r;
	r.h = r.w = 3;
	r.x = hive_x-1;
	r.y = hive_y-1;
	SDL_FillRect(screen, &r, SDL_MapRGB(screen->format, 0, 255, 0));
	r.x = food_x-1;
	r.y = food_y-1;
	SDL_FillRect(screen, &r, SDL_MapRGB(screen->format, 100, 100, 255));
}

void calculate_center_of_mass()
{
	int i;
	double x, y;
	for(i=0;i<NUM_BEES;i++)
	{
		x += creatures[i].x;
		y += creatures[i].y;
	}
	center_x = x / NUM_BEES;
	center_y = y / NUM_BEES;
}

void do_tick()
{
	int i;
	calculate_center_of_mass();
	for(i=0;i<NUM_BEES + NUM_BIRDS;i++)
	{
		//if(!creatures[i].dead)
			do_update(&creatures[i]);
	}
	draw_objects();
	SDL_Flip(screen);
}

void reset_game_state()
{
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
	hive_x = random() % WIDTH;
	hive_y = random() % HEIGHT;
	food_x = random() % WIDTH;
	food_y = random() % HEIGHT;
	center_x = WIDTH/2;
	center_y = HEIGHT/2;
	int i;
	for(i=0;i<NUM_BEES + NUM_BIRDS;i++)
	{
		memset(&creatures[i], 0, sizeof(struct organism));
		creatures[i].x = random() % WIDTH;
		creatures[i].y = random() % HEIGHT;
		creatures[i].start_x = creatures[i].x;
		creatures[i].start_y = creatures[i].y;
		creatures[i].goal_x = food_x;
		creatures[i].goal_y = food_y;
		creatures[i].health = HEALTH;
		creatures[i].type = i < NUM_BEES ? ORG_TYPE_BEE : ORG_TYPE_BIRD;
		creatures[i].genetics.accel = 2;
		creatures[i].genetics.speed = 3;
		creatures[i].genetics.attack = 2;
		creatures[i].genetics.aggressiveness = 2;
		creatures[i].genetics.claustrophobia = 2;
		creatures[i].genetics.determination = 15;
		creatures[i].genetics.friendliness = 1;
		creatures[i].genetics.frightfulness = 10;
	}
}

void set_random_genes(int type)
{
	int i;
	int s=0, e = NUM_BEES + NUM_BIRDS;
	if(type == ORG_TYPE_BIRD)
		s = NUM_BEES;
	if(type == ORG_TYPE_BEE)
		e = NUM_BEES;
	for(i=s;i<e;i++)
	{
		creatures[i].genetics.accel  = ((double)random() / RAND_MAX) * MAX;
		creatures[i].genetics.speed  = ((double)random() / RAND_MAX) * MAX_SPEED;
		creatures[i].genetics.attack = ((double)random() / RAND_MAX) * MAX;
		creatures[i].genetics.aggressiveness = ((double)random() / RAND_MAX) * MAX;
		creatures[i].genetics.claustrophobia = ((double)random() / RAND_MAX) * MAX;
		creatures[i].genetics.determination  = ((double)random() / RAND_MAX) * MAX;
		creatures[i].genetics.friendliness   = ((double)random() / RAND_MAX) * MAX;
		creatures[i].genetics.frightfulness  = ((double)random() / RAND_MAX) * MAX;
	}
}

void init(char *title)
{
	/* Initialise SDL Video */
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("Could not initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}
	/* Open a 640 x 480 screen */
	screen = SDL_SetVideoMode(WIDTH, HEIGHT, 0, SDL_HWPALETTE);
	if (screen == NULL)
	{
		printf("Couldn't set screen mode to 640 x 480: %s\n", SDL_GetError());
		exit(1);
	}
	/* Set the screen title */
	SDL_WM_SetCaption(title, NULL);
}

void cleanup()
{
	/* Shut down SDL */
	SDL_Quit();
}

void getInput()
{
	SDL_Event event;
	/* Loop through waiting messages and process them */
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			/* Closing the Window or pressing Escape will exit the program */
			case SDL_QUIT:
				exit(0);
			break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
					case SDLK_ESCAPE:
						exit(0);
					break;
					default:
					break;
				}
			break;
		}
	}
}

int calc_fitness(int type, struct organism *o)
{
	if(type == ORG_TYPE_BEE)
	{
		return o->kills * KILLS_REWARD + o->brought_food * FOOD_REWARD + 1;
	}
	return o->kills + o->health + 1;
}

void mutate(struct organism *o)
{
	double r = random();
	if(r / RAND_MAX < MUTATION_RATE)
	{
		int gene = random() % 8;
		float val = ((double)random() / RAND_MAX) * MAX_MUT;
		switch(gene) {
			case 0:
				o->genetics.accel = val;
				break;
			case 1:
				o->genetics.aggressiveness = val;
				break;
			case 2:
				o->genetics.attack = val;
				break;
			case 3:
				o->genetics.claustrophobia = val;
				break;
			case 4:
				o->genetics.determination = val;
				break;
			case 5:
				o->genetics.friendliness = val;
				break;
			case 6:
				o->genetics.frightfulness = val;
				break;
			case 7:
				o->genetics.speed = (val / MAX_MUT) * MAX_SPEED;
				break;
		}
	}
}

void selection()
{
	int num_alive_bees=0;
	int true_num_alive_bees=0;
	/* bees that haven't moved are considered dead */
	int i;
	int fit_bees=0;
	for(i=0;i<NUM_BEES;i++)
	{
		struct organism *o = &creatures[i];
		if(!o->dead && o->moved && o->brought_food) {
			o->fitness = calc_fitness(ORG_TYPE_BEE, o);
			fit_bees += o->fitness;
			num_alive_bees+=o->fitness;
			true_num_alive_bees++;
		}
	}
	
	if(!num_alive_bees) {
		printf("  randomizing genes (bees)\n");
		set_random_genes(ORG_TYPE_BEE);
	}
	
	struct organism parents_bees[num_alive_bees+1];
	int j=0;
	for(i=0;i<NUM_BEES && num_alive_bees;i++)
	{
		struct organism *o = &creatures[i];
		if(!o->dead && o->moved && o->brought_food) {
			int k;
			for(k=0;k<o->fitness;k++)
				memcpy(&parents_bees[j++], o, sizeof(*o));
		}
	}
	
	int num_alive_birds=0;
	int true_num_alive_birds=0;
	int fit_birds=0;
	/* bees that haven't moved are considered dead */
	for(i=NUM_BEES;i<NUM_BIRDS+NUM_BEES;i++)
	{
		struct organism *o = &creatures[i];
		if(!o->dead && o->moved) {
			o->fitness = calc_fitness(ORG_TYPE_BIRD, o);
			fit_birds += o->fitness;
			num_alive_birds+=o->fitness;
			true_num_alive_birds++;
		}
	}
	
	if(!num_alive_birds) {
		printf("  randomizing genes (birds)\n");
		set_random_genes(ORG_TYPE_BIRD);
	}
	
	struct organism parents_birds[num_alive_birds+1];
	j=0;
	for(i=NUM_BEES;i<NUM_BEES+NUM_BIRDS && num_alive_birds;i++)
	{
		struct organism *o = &creatures[i];
		if(!o->dead && o->moved) {
			int k;
			for(k=0;k<o->fitness;k++)
				memcpy(&parents_birds[j++], o, sizeof(*o));
		}
	}
	
	reset_game_state();
	
	for(i=0;i<NUM_BEES && num_alive_bees;i++) {
		int a, b;
		a = random() % num_alive_bees;
		b = random() % num_alive_bees;
		struct organism *o = &creatures[i];
		o->genetics.accel = parents_bees[a].genetics.accel;
		o->genetics.aggressiveness = parents_bees[a].genetics.aggressiveness;
		o->genetics.attack = parents_bees[a].genetics.attack;
		o->genetics.claustrophobia = parents_bees[a].genetics.claustrophobia;
		o->genetics.determination = parents_bees[b].genetics.determination;
		o->genetics.friendliness = parents_bees[b].genetics.friendliness;
		o->genetics.frightfulness = parents_bees[b].genetics.frightfulness;
		o->genetics.speed = parents_bees[b].genetics.speed;
		mutate(o);
	}
	
	for(i=NUM_BEES;i<NUM_BEES + NUM_BIRDS && num_alive_birds;i++) {
		int a, b;
		a = random() % num_alive_birds;
		b = random() % num_alive_birds;
		struct organism *o = &creatures[i];
		o->genetics.accel = parents_birds[a].genetics.accel;
		o->genetics.aggressiveness = parents_birds[a].genetics.aggressiveness;
		o->genetics.attack = parents_birds[a].genetics.attack;
		o->genetics.claustrophobia = parents_birds[a].genetics.claustrophobia;
		o->genetics.determination = parents_birds[b].genetics.determination;
		o->genetics.friendliness = parents_birds[b].genetics.friendliness;
		o->genetics.frightfulness = parents_birds[b].genetics.frightfulness;
		o->genetics.speed = parents_birds[b].genetics.speed;
		mutate(o);
	}
	
	if(num_alive_bees)
		printf("bee average fitness: %f. ", (double)fit_bees / true_num_alive_bees);
	if(num_alive_birds)
		printf("bird average fitness: %f. ", (double)fit_birds / true_num_alive_birds);
	
}

volatile int flag=0, alarm_count=0;
int generation=0;
void timeup(int s)
{
	alarm_count++;
	printf("\rgeneration %d: %d seconds left.\r", generation, GAME_LENGTH - alarm_count);
	if(alarm_count == GAME_LENGTH)
		flag=1;
	alarm(1);
}

int main(int argc, char **argv) {
	
	init("Swarm");
	setbuf(stdout, 0);
	srandom(time(NULL));
	atexit(cleanup);
	
	reset_game_state();
	set_random_genes(-1);
	
	signal(SIGALRM, timeup);
	
	while(1) {
		flag=0;
		alarm_count=0;
		generation++;
		printf("generation %d: ", generation);
		alarm(1);
		while (!flag)
		{
			getInput();
			do_tick();
			/* Sleep briefly to stop sucking up all the CPU time */
			SDL_Delay(DELAY);
		}
		alarm(0);
		printf("completed generation: %d. performing selection...\n", generation);
		selection();
		printf("\n");
	}
	
    return 0;
}
