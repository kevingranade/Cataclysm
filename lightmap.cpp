#include "mapdata.h"
#include "map.h"
#include "game.h"
#include "lightmap.h"

#define INBOUNDS(x, y) (x >= -SEEX && x <= SEEX && y >= -SEEY && y <= SEEY)
#define INBOUNDS_LARGE(x, y) (x >= -LIGHTMAP_RANGE_X && x <= LIGHTMAP_RANGE_X &&\
                               y >= -LIGHTMAP_RANGE_Y && y <= LIGHTMAP_RANGE_Y)

template <typename T, size_t X, size_t Y>
void fill(T (&array)[X][Y], T const& value)
{
 for(size_t x = 0; x < X; ++x) {
  for(size_t y = 0; y < Y; ++y) {
   array[x][y] = value;
  }
 }
}

light_map::light_map()
 : lm()
 , sm()
{
 memset(lm, 0, sizeof(lm));
 memset(sm, 0, sizeof(sm));
}

void light_map::generate(game* g, int x, int y, float natural_light, float luminance)
{
 build_light_cache(g, x, y);

 memset(lm, 0, sizeof(lm));
 memset(sm, 0, sizeof(sm));

 int dir_x[] = { 1, 0 , -1,  0 };
 int dir_y[] = { 0, 1 ,  0, -1 };
 int dir_d[] = { 180, 270, 0, 90 };

 // Daylight vision handling returned back to map due to issues it causes here
 if (natural_light > LIGHT_SOURCE_BRIGHT) {
  // Apply sunlight, first light source so just assign
  for(int sx = x - SEEX; sx <= x + SEEX; ++sx) {
   for(int sy = y - SEEY; sy <= y + SEEY; ++sy) {
    // In bright light indoor light exists to some degree
    if (!c[sx - x + LIGHTMAP_RANGE_X][sy - y + LIGHTMAP_RANGE_Y].is_outside)
     lm[sx - x + SEEX][sy - y + SEEY] = LIGHT_AMBIENT_LOW;
   }
  }
 }

 // Apply player light sources
 if (luminance > LIGHT_AMBIENT_LOW)
  apply_light_source(x, y, x, y, luminance);

 for(int sx = x - LIGHTMAP_RANGE_X; sx <= x + LIGHTMAP_RANGE_X; ++sx) {
  for(int sy = y - LIGHTMAP_RANGE_Y; sy <= y + LIGHTMAP_RANGE_Y; ++sy) {
   // When underground natural_light is 0, if this changes we need to revisit
   if (natural_light > LIGHT_AMBIENT_LOW) {
    if (!c[sx - x + LIGHTMAP_RANGE_X][sy - y + LIGHTMAP_RANGE_Y].is_outside) {
     // Apply light sources for external/internal divide
     for(int i = 0; i < 4; ++i) {
      if (INBOUNDS_LARGE(sx - x + dir_x[i], sy - y + dir_y[i]) &&
          c[sx - x + LIGHTMAP_RANGE_X + dir_x[i]][sy - y + LIGHTMAP_RANGE_Y + dir_y[i]].is_outside) {
       if (INBOUNDS(sx - x, sy - y) && c[LIGHTMAP_RANGE_X][LIGHTMAP_RANGE_Y].is_outside)
        lm[sx - x + SEEX][sy - y + SEEY] = natural_light;
       
       if (c[sx - x + LIGHTMAP_RANGE_X][sy - y + LIGHTMAP_RANGE_Y].transparency > LIGHT_TRANSPARENCY_SOLID)
       	apply_light_arc(sx, sy, dir_d[i], x, y, natural_light);
      }
	    }
    }
   }

   if (g->m.i_at(sx, sy).size() == 1 &&
       g->m.i_at(sx, sy)[0].type->id == itm_flashlight_on)
    apply_light_source(sx, sy, x, y, 20);
   
   if(g->m.ter(sx, sy) == t_lava)
    apply_light_source(sx, sy, x, y, 50);

   if(g->m.ter(sx, sy) == t_emergency_light)
    apply_light_source(sx, sy, x, y, 10);

   if(g->m.ter(sx, sy) == t_emergency_light_flicker && int(g->turn) % rng(2,7) == 150)
    apply_light_source(sx, sy, x, y, 10);

   // TODO: [lightmap] Attach light brightness to fields
   switch(g->m.field_at(sx, sy).type) {
    case fd_fire:
     if (3 == g->m.field_at(sx, sy).density)
      apply_light_source(sx, sy, x, y, 50);
     else if (2 == g->m.field_at(sx, sy).density)
      apply_light_source(sx, sy, x, y, 20);
     else
      apply_light_source(sx, sy, x, y, 8);
     break;
    case fd_fire_vent:
    case fd_flame_burst:
     apply_light_source(sx, sy, x, y, 8);
     break;
    case fd_electricity:
     if (3 == g->m.field_at(sx, sy).density)
      apply_light_source(sx, sy, x, y, 8);
     else if (2 == g->m.field_at(sx, sy).density)
      apply_light_source(sx, sy, x, y, 1);
     else
      apply_light_source(sx, sy, x, y, LIGHT_SOURCE_LOCAL);  // kinda a hack as the square will still get marked
     break;
   }

   // Apply any vehicle light sources
   if (c[sx - x + LIGHTMAP_RANGE_X][sy - y + LIGHTMAP_RANGE_Y].veh &&
       c[sx - x + LIGHTMAP_RANGE_X][sy - y + LIGHTMAP_RANGE_Y].veh_light > LL_DARK) {
    if (c[sx - x + LIGHTMAP_RANGE_X][sy - y + LIGHTMAP_RANGE_Y].veh_light > LL_LIT) {
     int dir = c[sx - x + LIGHTMAP_RANGE_X][sy - y + LIGHTMAP_RANGE_Y].veh->face.dir();
     float luminance = c[sx - x + LIGHTMAP_RANGE_X][sy - y + LIGHTMAP_RANGE_Y].veh_light;
     apply_light_arc(sx, sy, dir, x, y, luminance);
    }
   }

   if (c[sx - x + LIGHTMAP_RANGE_X][sy - y + LIGHTMAP_RANGE_Y].mon >= 0) {
    if (g->z[c[sx - x + LIGHTMAP_RANGE_X][sy - y + LIGHTMAP_RANGE_Y].mon].has_effect(ME_ONFIRE))
     apply_light_source(sx, sy, x, y, 3);

    // TODO: [lightmap] Attach natural light brightness to creatures
    // TODO: [lightmap] Allow creatures to have light attacks (ie: eyebot)
    // TODO: [lightmap] Allow creatures to have facing and arc lights
    switch(g->z[c[sx - x + LIGHTMAP_RANGE_X][sy - y + LIGHTMAP_RANGE_Y].mon].type->id) {
     case mon_zombie_electric:
      apply_light_source(sx, sy, x, y, 1);
      break;
     case mon_flaming_eye:
      apply_light_source(sx, sy, x, y, LIGHT_SOURCE_BRIGHT);
      break;
     case mon_manhack:
      apply_light_source(sx, sy, x, y, LIGHT_SOURCE_LOCAL);
      break;
    }
   }
  }
 }
}

lit_level light_map::at(int dx, int dy)
{
 if (!INBOUNDS(dx, dy))
  return LL_DARK; // Out of bounds

 if (sm[dx + SEEX][dy + SEEY] >= LIGHT_SOURCE_BRIGHT)
  return LL_BRIGHT;

 if (lm[dx + SEEX][dy + SEEY] >= LIGHT_AMBIENT_LIT)
  return LL_LIT;

 if (lm[dx + SEEX][dy + SEEY] >= LIGHT_AMBIENT_LOW)
  return LL_LOW;

 return LL_DARK;
}

float light_map::ambient_at(int dx, int dy)
{
 if (!INBOUNDS(dx, dy))
  return 0.0f;

 return lm[dx + SEEX][dy + SEEY];
}

bool light_map::is_outside(int dx, int dy)
{
 // We don't know and true seems a better default than false
 if (!INBOUNDS_LARGE(dx, dy))
  return true;

 return c[dx + LIGHTMAP_RANGE_X][dy + LIGHTMAP_RANGE_Y].is_outside;
}

bool light_map::sees(int fx, int fy, int tx, int ty, int max_range)
{
 if(!INBOUNDS_LARGE(fx, fy) || !INBOUNDS_LARGE(tx, ty))
  return false;

 if (max_range >= 0 && (abs(tx - fx) > max_range || abs(ty - fy) > max_range))
  return false; // Out of range!

 int ax = abs(tx - fx) << 1;
 int ay = abs(ty - fy) << 1;
 int dx = (fx < tx) ? 1 : -1;
 int dy = (fy < ty) ? 1 : -1;
 int x = fx;
 int y = fy;

 float transparency = LIGHT_TRANSPARENCY_CLEAR;

 // TODO: [lightmap] Pull out the common code here rather than duplication
 if (ax > ay) {
  int t = ay - (ax >> 1);
  do {
   if(t >= 0 && ((y + dy != ty) || (x + dx == tx))) {
    y += dy;
    t -= ax;
   }

   x += dx;
   t += ay;

   if(c[x + LIGHTMAP_RANGE_X][y + LIGHTMAP_RANGE_Y].transparency == LIGHT_TRANSPARENCY_SOLID)
    break;

  } while(!(x == tx && y == ty));
 } else {
  int t = ax - (ay >> 1);
  do {
   if(t >= 0 && ((x + dx != tx) || (y + dy == ty))) {
    x += dx;
    t -= ay;
   }

   y += dy;
   t += ax;

   if(c[x + LIGHTMAP_RANGE_X][y + LIGHTMAP_RANGE_Y].transparency == LIGHT_TRANSPARENCY_SOLID)
    break;

  } while(!(x == tx && y == ty));
 }

 return (x == tx && y == ty);
}

void light_map::apply_light_source(int x, int y, int cx, int cy, float luminance)
{
 bool lit[LIGHTMAP_X][LIGHTMAP_Y];
 memset(lit, 0, sizeof(lit));

 if (INBOUNDS(x - cx, y - cy)) {
  lit[x - cx + SEEX][y - cy + SEEY] = true;
  lm[x - cx + SEEX][y - cy + SEEY] += std::max(luminance, static_cast<float>(LL_LOW));
  sm[x - cx + SEEX][y - cy + SEEY] += luminance;
 }

 if (luminance > LIGHT_SOURCE_LOCAL) {
  int range = LIGHT_RANGE(luminance);
  int sx = x - cx - range; int ex = x - cx + range;
  int sy = y - cy - range; int ey = y - cy + range;

  for(int off = sx; off <= ex; ++off) {
   apply_light_ray(lit, x, y, cx + off, cy + sy, cx, cy, luminance);
   apply_light_ray(lit, x, y, cx + off, cy + ey, cx, cy, luminance);
  }

  // Skip corners with + 1 and < as they were done
  for(int off = sy + 1; off < ey; ++off) {
   apply_light_ray(lit, x, y, cx + sx, cy + off, cx, cy, luminance);
   apply_light_ray(lit, x, y, cx + ex, cy + off, cx, cy, luminance);
  }
 }
}

void light_map::apply_light_arc(int x, int y, int angle, int cx, int cy, float luminance)
{
 if (luminance <= LIGHT_SOURCE_LOCAL)
  return;

 bool lit[LIGHTMAP_X][LIGHTMAP_Y];
 memset(lit, 0, sizeof(lit));

 int range = LIGHT_RANGE(luminance);
 apply_light_source(x, y, cx, cy, LIGHT_SOURCE_LOCAL);

 // Normalise (should work with negative values too)
 angle = angle % 360;

 // East side
 if (angle < 90 || angle > 270) {
  int sy = y - cy - ((angle <  90) ? range * (( 45 - angle) / 45.0f) : range);
  int ey = y - cy + ((angle > 270) ? range * ((angle - 315) / 45.0f) : range);

  int ox = x - cx + range;
  for(int oy = sy; oy <= ey; ++oy)
   apply_light_ray(lit, x, y, cx + ox, cy + oy, cx, cy, luminance);
 }

 // South side
 if (angle < 180) {
  int sx = x - cx - ((angle < 90) ? range * (( angle - 45) / 45.0f) : range);
  int ex = x - cx + ((angle > 90) ? range * ((135 - angle) / 45.0f) : range);

  int oy = y - cy + range;
  for(int ox = sx; ox <= ex; ++ox)
   apply_light_ray(lit, x, y, cx + ox, cy + oy, cx, cy, luminance);
 }

 // West side
 if (angle > 90 && angle < 270) {
  int sy = y - cy - ((angle < 180) ? range * ((angle - 135) / 45.0f) : range);
  int ey = y - cy + ((angle > 180) ? range * ((225 - angle) / 45.0f) : range);

  int ox = x - cx - range;
  for(int oy = sy; oy <= ey; ++oy)
   apply_light_ray(lit, x, y, cx + ox, cy + oy, cx, cy, luminance);
 }

 // North side
 if (angle > 180) {
  int sx = x - cx - ((angle > 270) ? range * ((315 - angle) / 45.0f) : range);
  int ex = x - cx + ((angle < 270) ? range * ((angle - 225) / 45.0f) : range);

  int oy = y - cy - range;
  for(int ox = sx; ox <= ex; ++ox)
   apply_light_ray(lit, x, y, cx + ox, cy + oy, cx, cy, luminance);
 }
}

void light_map::apply_light_ray(bool lit[LIGHTMAP_X][LIGHTMAP_Y], int sx, int sy,
                                int ex, int ey, int cx, int cy, float luminance)
{
 int ax = abs(ex - sx) << 1;
 int ay = abs(ey - sy) << 1;
 int dx = (sx < ex) ? 1 : -1;
 int dy = (sy < ey) ? 1 : -1;
 int x = sx;
 int y = sy;

 float transparency = LIGHT_TRANSPARENCY_CLEAR;

 // TODO: [lightmap] Pull out the common code here rather than duplication
 if (ax > ay) {
  int t = ay - (ax >> 1);
  do {
   if(t >= 0) {
    y += dy;
    t -= ax;
   }

   x += dx;
   t += ay;

   if (INBOUNDS(x - cx, y - cy) && !lit[x - cx + SEEX][y - cy + SEEY]) {
    // Multiple rays will pass through the same squares so we need to record that
    lit[x - cx + SEEX][y - cy + SEEY] = true;

    // We know x is the longest angle here and squares can ignore the abs calculation
    float light = luminance / ((sx - x) * (sx - x));
    lm[x - cx + SEEX][y - cy + SEEY] += light * transparency;
   }

   if (INBOUNDS_LARGE(x - cx, y - cy))
    transparency *= c[x - cx + LIGHTMAP_RANGE_X][y - cy + LIGHTMAP_RANGE_Y].transparency;

   if (transparency <= LIGHT_TRANSPARENCY_SOLID)
    break;

  } while(!(x == ex && y == ey));
 } else {
  int t = ax - (ay >> 1);
  do {
   if(t >= 0) {
    x += dx;
    t -= ay;
   }
 
   y += dy;
   t += ax;

   if (INBOUNDS(x - cx, y - cy) && !lit[x - cx + SEEX][y - cy + SEEY]) {
    // Multiple rays will pass through the same squares so we need to record that
    lit[x - cx + SEEX][y - cy + SEEY] = true;

    // We know y is the longest angle here and squares can ignore the abs calculation
    float light = luminance / ((sy - y) * (sy - y));
    lm[x - cx + SEEX][y - cy + SEEY] += light;
   }

   if (INBOUNDS_LARGE(x - cx, y - cy))
    transparency *= c[x - cx + LIGHTMAP_RANGE_X][y - cy + LIGHTMAP_RANGE_Y].transparency;

   if (transparency <= LIGHT_TRANSPARENCY_SOLID)
    break;

  } while(!(x == ex && y == ey));
 }
}

// We only do this once now so we don't make 100k calls to is_outside for each
// generation. As well as close to that for the veh_at function.
void light_map::build_light_cache(game* g, int cx, int cy)
{
 // Clear cache
 for(int sx = cx - LIGHTMAP_RANGE_X; sx <= cx + LIGHTMAP_RANGE_X; ++sx) {
  for(int sy = cy - LIGHTMAP_RANGE_Y; sy <= cy + LIGHTMAP_RANGE_Y; ++sy) {
   int x = sx - cx + LIGHTMAP_RANGE_X;
   int y = sy - cy + LIGHTMAP_RANGE_Y;

   c[x][y].is_outside = g->m.is_outside(sx, sy);
   c[x][y].transparency = LIGHT_TRANSPARENCY_CLEAR;
   c[x][y].veh = NULL;
   c[x][y].veh_part = 0;
   c[x][y].veh_light = 0;
   c[x][y].mon = -1;
  }
 }

 // Check for critters and cache
 for (int i = 0; i < g->z.size(); ++i)
  if (INBOUNDS(g->z[i].posx - cx, g->z[i].posy - cy))
   c[g->z[i].posx - cx + LIGHTMAP_RANGE_X][g->z[i].posy - cy + LIGHTMAP_RANGE_Y].mon = i;

 // Check for vehicles and cache
 VehicleList vehs = g->m.get_vehicles(cx - LIGHTMAP_RANGE_X, cy - LIGHTMAP_RANGE_Y, cx + LIGHTMAP_RANGE_X, cy + LIGHTMAP_RANGE_Y);
 for(int v = 0; v < vehs.size(); ++v) {
  for(int p = 0; p < vehs[v].item->parts.size(); ++p) {
   int px = vehs[v].x + vehs[v].item->parts[p].precalc_dx[0] - cx;
   int py = vehs[v].y + vehs[v].item->parts[p].precalc_dy[0] - cy;
      
   if (INBOUNDS(px, py)) {
    px += LIGHTMAP_RANGE_X;
    py += LIGHTMAP_RANGE_Y;

    // External part appears to always be the first?
    if (!c[px][py].veh) {
     c[px][py].veh = vehs[v].item;
     c[px][py].veh_part = p;
    }

    if (vehs[v].item->lights_on &&
        vehs[v].item->part_flag(p, vpf_light) &&
        vehs[v].item->parts[p].hp > 0)
     c[px][py].veh_light = vehs[v].item->part_info(p).power;
   }
  }
 }

 for(int sx = cx - LIGHTMAP_RANGE_X; sx <= cx + LIGHTMAP_RANGE_X; ++sx) {
  for(int sy = cy - LIGHTMAP_RANGE_Y; sy <= cy + LIGHTMAP_RANGE_Y; ++sy) {
   int x = sx - cx + LIGHTMAP_RANGE_X;
   int y = sy - cy + LIGHTMAP_RANGE_Y;

   if (c[x][y].veh) {
    if (c[x][y].veh->part_flag(c[x][y].veh_part, vpf_opaque) &&
        c[x][y].veh->parts[c[x][y].veh_part].hp > 0) {
     int dpart = c[x][y].veh->part_with_feature(c[x][y].veh_part, vpf_openable);
     if (dpart < 0 || !c[x][y].veh->parts[dpart].open) {
      c[x][y].transparency = LIGHT_TRANSPARENCY_SOLID;
      continue;
     }
    }
   } else if (!(terlist[g->m.ter(sx, sy)].flags & mfb(transparent))) {
    c[x][y].transparency = LIGHT_TRANSPARENCY_SOLID;
    continue;
   }

   field& f = g->m.field_at(sx, sy);
   if(f.type > 0) {
    if(!fieldlist[f.type].transparent[f.density - 1]) {
     // Fields are either transparent or not, however we want some to be translucent
     switch(f.type) {
      case fd_smoke:
      case fd_toxic_gas:
      case fd_tear_gas:
       if(f.density == 3)
        c[x][y].transparency = LIGHT_TRANSPARENCY_SOLID;
       if(f.density == 2)
        c[x][y].transparency *= 0.5;
       break;
      case fd_nuke_gas:
       c[x][y].transparency *= 0.5;
       break;
      default:
       c[x][y].transparency = LIGHT_TRANSPARENCY_SOLID;
       break;
     }
    }

    // TODO: [lightmap] Have glass reduce light as well
   }
  }
 }
}
