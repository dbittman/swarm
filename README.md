swarm
=====

Playing around with genetic algorithms

World Rules
-----------
There are two types of organisms: birds and bees. Birds eat bees, but bees can sting birds.
If a bird gets stung enough, it dies. If a bee touches the front of a bird, it gets eaten.
Organisms make a move toward a 'target'. This target is selected by looking at all possible
actions, and choosing one. These include:
 * Avoiding an enemy (frightfulness)
 * Grouping with similar organisms (friendliness)
 * Keeping some "personal space" (claustrophobia)
 * Attacking an enemy (aggressiveness)
 * Seeking a goal (determination)
 
Each of the words in parenthesies is also a gene.

These are selected based on distance to the 'target' for each of these categories, and how
important that specific creature thinks that category is. For each category, the target is
defined (closest neighbor for friendliness, nearest bird if you're a bee for aggressiveness).
The score for that category is calculated by doing (MAX_DISTANCE-distance_to_target) * weight_for_that_category,
where MAX_DISTANCE is the largest distance in that world (corner to corner). The category with
the highest score is selected.

There are some exceptions: claustrophobia can only take effect if the neighboring organism is within
3 pixels. In addition, aggressiveness is amplified at close range: All of these scores respond inversely linearly
with distance from the target. Aggressiveness responds like an inverse square law at short distances (if you get really
close to an enemy, you become VERY aggressive).

Once the category is selected, it is decided whether or not to approach the target or to run away. For example, for
firghtfulness, it would be selected to run away. Then, the acceleration of the organism is set to be a vector
towards (or away from) the target, with a magnitude set to the organisms 'accel' gene.

Birds only respond to claustrophobia and aggressiveness.

If a bee is along the edges of the world, their target is overridden to be the hive: explorers must return home
to report their findings.

There exist two special locations in the world: the food square and the hive. Bees have a goal, to move food from the
food square (of which there is an unlimited amount) to the hive. They do this by touching the food square. When they do
this, their goal is updated to be the hive's coordinates. When they get to the hive, their goal is updated to be the food
square, and this repeats. If they get food to the hive, a counter increases for that bee.

If a bird eats a bee, it heals by an amount defined by a constant. If the bee was carrying food, it heals twice as much.

Each game 'tick', every organism loses velocity by an amount defined as 'AIR_RESIST'. Then, the organism's acceleration is
added to the velocity (vector addition). If the new velocity's magnitude is higher than the organism's 'speed gene', then
the addition doesn't take place. The velocity is then added to the organism's position (and modified to keep it in the
game world).

The game starts at generation 1 with all random positions and random genetics. After GAME_LENGTH seconds, the game ends, 
and the selection of genes for the next round takes place. The game is then re-initialized with random positions, but now
non-random genetics.

Selection
---------
Only bees that are alive, have moved, and have ferried food back to the hive may pass on their genes. All other bees
are considered dead. Birds are similar, except that all that is looked at is "is alive and has moved". For each
new organism, it chooses two random parents (of the same type of organism) and uses half the genes from one, and half
from the other. An organism with a higher 'fitness' score is more likely to be chosen as a parent than others. After
the genes are merged, there is a small chance for a random gene to become a random number (mutation).
