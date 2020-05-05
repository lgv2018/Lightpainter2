/*
 *  Menu draw functions for lightpainter2
 *
 *  These are optimized for a 126x32 OLED display.
 */

void title() {
  u8g2.setFont(u8g2_font_maniac_tr);
  u8g2.drawStr(scrollPos,24,"Lightpainter2");
  if (currentMillis - timePast > 15 ){
    timePast=currentMillis;
    scrollPos-=1;
    if (scrollPos < -255) scrollPos = 0;
  }
}

void menu_start() {
  u8g2.setFont(u8g2_font_profont17_tr);
  u8g2.drawStr(10,30,"Start");
  u8g2.drawStr(80,30,"Menu");
  if (selected == 0) u8g2.drawRFrame(0,16,64,16,3);
  if (selected == 1) u8g2.drawRFrame(64,16,64,16,3);
  //filename and symbol
  u8g2.setFont(u8g2_font_open_iconic_mime_1x_t);
  u8g2.drawGlyph(1,9,73);
  u8g2.setFont(u8g2_font_t0_12_tr); //u8g2_font_5x7_tr);
  u8g2.drawStr(10,9,fileIndex[cImage]);
}

void menu_main_layout( const char *item, uint8_t pos) {
  u8g2.drawStr(64-(u8g2.getStrWidth(item)/2),16,item); u8g2.drawDisc(pos, 28, 3, U8G2_DRAW_ALL);
  if (pos > 40)  { u8g2.setFont(u8g2_font_calibration_gothic_nbp_tr); u8g2.drawStr(0,26,"<");}
  if (pos < 80)  { u8g2.setFont(u8g2_font_calibration_gothic_nbp_tr); u8g2.drawStr(122,26,">");}
}

void menu_main() {
  u8g2.setFont(u8g2_font_helvB12_tf);
  for (uint8_t i=0; i<5; i++) {
    u8g2.drawCircle(30+(i*15), 28, 3, U8G2_DRAW_ALL);
  }
  switch (selected) {
    case 2: menu_main_layout("Select file",30); break;
    case 3: menu_main_layout("Brightness",45); break;
    case 4: menu_main_layout("Speed",60); break;
    case 5: menu_main_layout("Delay",75); break;
    case 6: menu_main_layout("Save settings",90); break;
  }
}

void select_file() {

  // let's calculate display limits; we can only fit 3 on screen
  uint8_t minIndex = (cImage > 1) ? cImage - 1 : 0 ;
  uint8_t maxIndex = minIndex + 2;
  if (maxIndex > nImages - 1) { maxIndex = nImages - 1; minIndex = nImages - 3; }

  // output strings and scroll selected
  u8g2.setFont(u8g2_font_t0_12_tr);
  for (uint8_t i=minIndex; i<= maxIndex; i++){
    if (i == cImage && u8g2.getStrWidth(fileIndex[i]) > 120) {
      if (scrollPos < 110 - u8g2.getStrWidth(fileIndex[i])) scrollPos = 0;
      u8g2.drawStr(scrollPos,10*(i-minIndex+1),fileIndex[i]);
    }else{
      u8g2.drawStr(2,10*(i-minIndex+1),fileIndex[i]);
    }
  }
  // mark selected
  // unless current Image is 0 or fileNum, bar will always be in the middle
  uint8_t selecBar = 11;
  if (cImage == 0) selecBar = 1;
  if (cImage == nImages) selecBar = 21; // nImages will be images-1 because we start with 0. This might be the reason for file selection bug. To Do!
  u8g2.setDrawColor(2);
  u8g2.drawBox(1,selecBar,120,10);
  u8g2.setDrawColor(1);

  // draw the scroll bar
  u8g2.drawFrame(122,0,6,32); // border
  float boxHeight = (nImages > 2) ? 3.00 / (nImages - 1 ) * 28 : 28 ;
  float boxPos = (nImages > 2) ? 1.00 / (nImages - 1 ) * (28 - boxHeight) * cImage : 0 ;

  u8g2.drawBox(124,2+boxPos,2,boxHeight); // box

  // change scroll timer
  if (currentMillis - scrollTime > 15 ){
    scrollTime=currentMillis;
    scrollPos-=1;
  }
}

void select_level() {
  uint8_t val;
  char item[12];
  if (selected == 3){
    char item[] = "Brightness";
    val = map (ledBrightness, 0,255,0,120);
  } else if (selected == 4) {
    char item[] = "Speed";
    val = map (Speed, 0,255,0,120);
  } else if (selected == 5) {
    char item[] = "Delay";
    val = map (Delay, 0,255,0,120);
  }
  u8g2.drawFrame(3,0,122,15);
  u8g2.setFont(u8g2_font_helvB10_tf);
  u8g2.drawStr(64-(u8g2.getStrWidth(item)/2),12,item);
  u8g2.drawStr (82,30,"Save");
  u8g2.drawFrame(73,17,52,15);
  u8g2.setCursor(3,32);
  u8g2.print(val);
  u8g2.setDrawColor(2);
  u8g2.drawBox(4,1,val,13);
  u8g2.setDrawColor(1);
}

void save_settings() {
  u8g2.setFont(u8g2_font_open_iconic_check_2x_t);
  u8g2.drawGlyph(10,32,67);
  u8g2.setFont(u8g2_font_profont17_tr);
  u8g2.drawStr(40,20,"Settings");
  u8g2.drawStr(40,32,"Saved");
}
