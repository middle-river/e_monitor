#!/usr/bin/python3
# Generate a binary image for the environment monitor.

from PIL import Image, ImageDraw, ImageFont
import datetime
import io
import matplotlib.dates
import matplotlib.pyplot as plt
import time

WIDTH = 758
HEIGHT = 1024
STATSIZE = 40
FONTSIZE = 80
PWIDTH = WIDTH // 2
PHEIGHT = (HEIGHT - STATSIZE) // 4
FONT = '/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf'
WATFILE = '/var/log/sm.log'
WSTFILE = '/var/log/wst.log'
OUTFILE = '/var/www/html/share/e_monitor/mon.tif'

class Text:
  def __init__(self, image, draw, size, font=FONT):
    self._image = image
    self._draw = draw
    self._size = size
    self._font = ImageFont.truetype(font, size=size)
  def put(self, x, y, text, fill):
    self._draw.text((x, y), text, fill=fill, font=self._font)

def chart(title, width, height, xy0, xy1=None, xy2=None):
  fig, ax = plt.subplots()
  plt.subplots_adjust(left=0.12, right=0.98)
  plt.style.use('grayscale')
  fig.set_size_inches(width / 100, height / 100)
  fig.autofmt_xdate(rotation=45)
  ax.set_title(title, fontsize=10)
  ax.grid(axis='y', color='black', linestyle='dotted')
  ax.xaxis.set_major_formatter(matplotlib.dates.DateFormatter('%H:%M'))
  plt.yticks(fontsize=8)
  if xy0:
    x, y = zip(*xy0)
    ax.plot(x, y, color='black', linewidth=2.0, linestyle='solid')
  if xy1:
    x, y = zip(*xy1)
    ax.plot(x, y, color='black', linewidth=2.0, linestyle='dashed')
  if xy2:
    x, y = zip(*xy2)
    ax.plot(x, y, color='black', linewidth=3.0, linestyle='dotted')
  f = io.BytesIO()
  plt.savefig(f, dpi=100)
  f.seek(0)
  return Image.open(f)

def latest_value(now, data, default):
  return default if now - data[-1][0] > datetime.timedelta(minutes=10) else data[-1][1]

def get_power(stats):
  now = datetime.datetime.now()
  # Read the log file.
  LINELEN = 45
  with open(WATFILE, 'r') as f:
    f.seek(max(f.tell() - LINELEN * 24 * 60 // 5, 0), 0)
    f.readline()
    data = f.readlines()
  data = [d.split() for d in data]
  watt = []
  for time, host, vals in data:
    dt = datetime.datetime.fromisoformat(time).replace(tzinfo=None)
    if now - dt > datetime.timedelta(days=1):
      continue
    watt.append((dt, float(vals)))
  # Draw the chart.
  charts = []
  charts.append(chart('Power (W)', PWIDTH, PHEIGHT, watt))
  stats['watt'] = latest_value(now, watt, 9999.9)
  return charts

def get_weather(stats):
  now = datetime.datetime.now()
  # Read the log file.
  LINELEN = 70
  with open(WSTFILE, 'r') as f:
    f.seek(max(f.tell() - LINELEN * 24 * 60 // 5 * 3, 0), 0)
    f.readline()
    data = f.readlines()
  data = [d.split() for d in data]
  pres = []
  temp = ([], [], [])
  humi = ([], [], [])
  batt = ([], [], [])
  for time, host, vals in data:
    dt = datetime.datetime.fromisoformat(time).replace(tzinfo=None)
    if now - dt > datetime.timedelta(days=1):
      continue
    vals = vals.split(',')
    stid = int(vals[0])
    assert stid >= 0 and stid <= 2
    temp[stid].append((dt, float(vals[1])))
    humi[stid].append((dt, float(vals[2])))
    if stid == 0:
      pres.append((dt, float(vals[3])))
    batt[stid].append((dt, float(vals[4])))
  # Draw the chart.
  charts = []
  charts.append(chart('Pressure (hPa)',   PWIDTH, PHEIGHT, pres))
  charts.append(chart('Temperature (℃)', PWIDTH, PHEIGHT, temp[0], temp[1], temp[2]))
  charts.append(chart('Humidity (%)',     PWIDTH, PHEIGHT, humi[0], humi[1], humi[2]))
  stats['pres'] = latest_value(now, pres, 9999.9)
  stats['temp0'] = latest_value(now, temp[0], 99.9)
  stats['temp1'] = latest_value(now, temp[1], 99.9)
  stats['temp2'] = latest_value(now, temp[2], 99.9)
  stats['humi0'] = latest_value(now, humi[0], 999.9)
  stats['humi1'] = latest_value(now, humi[1], 999.9)
  stats['humi2'] = latest_value(now, humi[2], 999.9)
  stats['batt0'] = latest_value(now, batt[0], 9.99)
  stats['batt1'] = latest_value(now, batt[1], 9.99)
  stats['batt2'] = latest_value(now, batt[2], 9.99)
  return charts

def main():
  # Initialize
  image = Image.new('1', (WIDTH, HEIGHT), color=255)
  draw = ImageDraw.Draw(image)
  stat = Text(image, draw, STATSIZE)
  text = Text(image, draw, FONTSIZE)

  # Plot the charts.
  charts = []
  stats = {}
  charts += get_power(stats)
  charts += get_weather(stats)
  for i in range(4):
    chart = charts[i].convert(mode='1', dither=Image.Dither.NONE)
    image.paste(chart, (PWIDTH * 0, STATSIZE + PHEIGHT * i))

  # Draw the text.
  text.put(WIDTH / 2 + 100, STATSIZE + 248 * 0 + (248 - FONTSIZE) / 2, '%.0fW' % stats['watt'], 0)
  text.put(WIDTH / 2 + 40, STATSIZE + 248 * 1 + (248 - FONTSIZE) / 2, '%.0fhPa' % stats['pres'], 0)
  for i in range(3):
    text.put(WIDTH / 2 + 80, STATSIZE + 248 * 2 + 2 + (FONTSIZE + 2) * i, '%.1f℃' % stats['temp%d' % i], 0)
    x = WIDTH / 2 + 80 + 40 * 4
    y = STATSIZE + 248 * 2 + 2 + (FONTSIZE + 2) * i
  for i in range(3):
    text.put(WIDTH / 2 + 120, STATSIZE + 248 * 3 + 2 + (FONTSIZE + 2) * i, '%.0f%%' % stats['humi%d' % i], 0)

  # Plot the status.
  now = datetime.datetime.now()
  date = now.strftime('%Y-%m-%d')
  dotw = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'][now.weekday()]
  time = now.strftime('%H:%M')
  status = '{batt0:3.1f}V {batt1:3.1f}V {batt2:3.1f}V'.format(**stats)
  draw.line((0, STATSIZE, WIDTH - 1, STATSIZE), fill=0, width=1)
  stat.put(1, 0, '%s %s %s  %s' % (date, dotw, time, status), 0)

  # Save images
  image = image.rotate(-90, expand=True)
  image.save(OUTFILE, compression='group4', tiffinfo={262: 0})

if __name__ == '__main__':
  main()
