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
FONTSIZE = 32
PWIDTH = WIDTH // 2
PHEIGHT = (HEIGHT - FONTSIZE) // 4
FONT = '/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf'
WATFILE = '/usr/local/lib/smartmeter/sm.dat'
WSTFILE = '/usr/lib/cgi-bin/wst.dat'
OUTFILE = '/var/www/html/e_monitor/mon.tif'

class Text:
  def __init__(self, image, draw, font=FONT, size=FONTSIZE):
    self._image = image
    self._draw = draw
    self._font = ImageFont.truetype(font, size=size)
    self._size = size
  def put(self, x, y, text, fill):
    self._draw.text((x, y), text, fill=fill, font=self._font)

def chart(title, fmt, width, height, x0, y0, x1=None, y1=None):
  fig, ax = plt.subplots()
  plt.subplots_adjust(left=0.12, right=0.98)
  plt.style.use('grayscale')
  fig.set_size_inches(width / 100, height / 100)
  fig.autofmt_xdate(rotation=45)
  ax.set_title(title, fontsize=10)
  ax.grid(axis='y', color='black', linestyle='dotted')
  ax.xaxis.set_major_formatter(matplotlib.dates.DateFormatter(fmt))
  plt.yticks(fontsize=8)
  if x0 and y0:
    ax.plot(x0, y0, color='black', linewidth=1.0, linestyle='solid')
  if x1 and y1:
    ax.plot(x1, y1, color='black', linewidth=1.0, linestyle='dashed')
  f = io.BytesIO()
  plt.savefig(f, dpi=100)
  f.seek(0)
  return Image.open(f)

def get_power(states):
  with open(WATFILE, 'r') as f:
    data = f.readlines()
  data = [d.split(',') for d in data]
  # Daily stats.
  daily_x = []
  daily_y = []
  for time, watt in data[-24 * 12:]:
    daily_x.append(datetime.datetime.fromisoformat(time))
    daily_y.append(float(watt))
  # Weekly stats.
  weekly_x = []
  weekly_y = []
  n = len(data)
  n = (n // 6) * 6
  data = data[-n:]
  for i in range(0, n, 6):
    weekly_x.append(datetime.datetime.fromisoformat(data[i][0]))
    watt = sum([int(d[1]) for d in data[i:i + 6]]) / 6
    weekly_y.append(watt)
  charts = []
  charts.append(chart('Power (W)', '%H:%M', PWIDTH, PHEIGHT, daily_x, daily_y))
  charts.append(chart('Power (W)', '%m/%d', PWIDTH, PHEIGHT, weekly_x, weekly_y))
  states['power'] = daily_y[-1]
  return charts

def get_weather(states):
  def read_data(text, origin):
    # This data structure contains only the data necessary for rendering recent weekly/daily charts.
    weekly = [[''] * 24 * 7, [''] * 24 * 7]	# hourly data for a week.
    daily = [[''] * 6 * 24, [''] * 6 * 24]	# 10 minute interval data for a day.
    for buf in text.split('\n'):
      if not buf:
        continue
      time, cid, temp, humi, pres, volt = buf.split(',')
      cid = int(cid)
      assert cid == 0 or cid == 1
      dt = datetime.datetime.fromisoformat(time)
      wdiff = origin.replace(minute=0, second=0, microsecond=0) - dt.replace(minute=0, second=0, microsecond=0)
      w = int(wdiff.total_seconds() / (60 * 60))
      ddiff = origin.replace(minute=origin.minute // 10 * 10, second=0, microsecond=0) - dt.replace(minute=dt.minute // 10 * 10, second=0, microsecond=0)
      d = int(ddiff.total_seconds() / (60 * 10))
      if w < 24 * 7 and not weekly[cid][-w - 1]:
        weekly[cid][-w - 1] = buf
      if d < 6 * 24 and not daily[cid][-d - 1]:
        daily[cid][-d - 1] = buf
    return (weekly, daily)

  # Read the data.
  now = datetime.datetime.now()
  with open(WSTFILE, 'r') as f:
    data = read_data(f.read(), now)
  values = [[[[] for _ in range(5)] for _ in range(2)] for _ in range(2)]	# values[weekly/daily][cid][time/temp/humi/pres/volt]
  for span in range(2):
    for cid in range(2):
      for buf in data[span][cid]:
        if not buf:
          continue
        time, _, temp, humi, pres, volt = buf.split(',')
        values[span][cid][0].append(datetime.datetime.fromisoformat(time))
        values[span][cid][1].append(float(temp))
        values[span][cid][2].append(float(humi))
        if pres:
          values[span][cid][3].append(float(pres))
        values[span][cid][4].append(float(volt))

  # Plot the charts.
  charts = []
  for i, title in enumerate(['Temperature (℃)', 'Humidity (%)', 'Pressure (hPa)']):
    charts.append(chart(title, '%H:%M', PWIDTH, PHEIGHT, values[1][1][0], values[1][1][1 + i], values[1][0][0], values[1][0][1 + i]))
    charts.append(chart(title, '%m/%d', PWIDTH, PHEIGHT, values[0][1][0], values[0][1][1 + i], values[0][0][0], values[0][0][1 + i]))
  states['temp'] = values[1][1][1][-1]
  states['humi'] = values[1][1][2][-1]
  states['batt0'] = values[1][1][4][-1]
  states['batt1'] = values[1][0][4][-1]
  return charts

def main():
  # Initialize
  image = Image.new('1', (WIDTH, HEIGHT), color=255)
  draw = ImageDraw.Draw(image)
  text = Text(image, draw)

  # Plot the charts.
  charts = []
  states = {}
  charts += get_power(states)
  charts += get_weather(states)
  for i in range(4):
    for j in range(2):
      chart = charts[i * 2 + j].convert(mode='1', dither=Image.Dither.NONE)
      image.paste(chart, (PWIDTH * j, FONTSIZE + PHEIGHT * i))

  # Plot the status.
  now = datetime.datetime.now()
  date = now.strftime('%Y-%m-%d')
  dotw = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'][now.weekday()]
  time = now.strftime('%H:%M')
  status = '{power:4.0f}W {temp:4.1f}C {humi:2.0f}% {batt0:3.1f}V {batt1:3.1f}V'.format(**states)
  draw.line((0, FONTSIZE, WIDTH - 1, FONTSIZE), fill=0, width=1)
  text.put(1, 0, '%s %s %s %s' % (date, dotw, time, status), 0)
  draw.ellipse((16 * 31 + 1, 1, 16 * 31 + 4, 4), fill=255, outline=0)	# Hack for Celsius degree.

  # Save images
  image = image.rotate(-90, expand=True)
  image.save(OUTFILE, compression='group4', tiffinfo={262: 0})

if __name__ == '__main__':
  main()
