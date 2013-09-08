
package com.zevv.fwloc;

import java.net.DatagramSocket;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.net.SocketException;
import java.io.IOException;
import java.util.concurrent.ExecutionException;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.location.Criteria;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.media.AudioFormat;
import android.media.AudioRecord; 
import android.os.AsyncTask;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.widget.TextView;
import android.widget.Toast;
import android.provider.Settings;

public class fwloc extends Activity implements LocationListener
{
	private Canvas canvas = new Canvas();
	private int w = 0, h = 0;
	private GraphView gv;
	private Bitmap bitmap;
	private AudioTask audiotask;
	private NtpTask ntptask;
	private DatagramSocket sock;
	private AudioRecord rec;
	private int srate = 8000;
	private short[] buffer;
	private int buflen;
	private Paint paint = new Paint(0);
	private int py = 0;
	private int x = 0;
	private double pv = 0;
	private double vavg = 0;
	private double clock_offset = 0;
	private LocationManager locationManager;
	private Location location;
	private double a = 0;
	private String host = "fwloc.zevv.nl";
	private String device_id = "?";

	/* 
	 * Send event to server in UDP message 
	 */

	private void send_event(String msg)
	{
		try {
			msg = "i:" + device_id +  " " + msg;
			InetAddress local = InetAddress.getByName(host);
			int msg_length = msg.length();
			byte[] message = msg.getBytes();
			DatagramPacket p = new DatagramPacket(message, msg_length, local, 9339);
			sock.send(p);
		} catch(UnknownHostException e) {
			Toast.makeText(this, "Unknown host: " + e, Toast.LENGTH_SHORT);
		} catch(IOException e) {
			Toast.makeText(this, "IO error " + e, Toast.LENGTH_SHORT);
		};
	}

	/*
	 * Audio view
	 */

	private class GraphView extends View 
	{

		public GraphView(Context context) {
			super(context);
		}


		@Override
		protected void onSizeChanged(int nw, int nh, int pw, int ph) 
		{
			w = nw;
			h = nh;
			bitmap = Bitmap.createBitmap(nw, nh, Bitmap.Config.RGB_565);
			canvas.setBitmap(bitmap);
			super.onSizeChanged(w, h, pw, ph);
			//Log.i("fwloc", "sizeChanged");
		}


		@Override
		protected void onDraw(Canvas canvas) 
		{
			if(bitmap != null) {
				canvas.drawBitmap(bitmap, 0, 0, null);
			}
		}
	}


	/*
	 * NTP sync task
	 */

	private class NtpTask extends AsyncTask<Void, Double, Integer> 
	{
	
		@Override
		protected Integer doInBackground(Void... params) 
		{
			while(! isCancelled()) {

				try {
					DatagramSocket socket = new DatagramSocket();
					InetAddress address = InetAddress.getByName(host);
					byte[] buf = new NtpMessage().toByteArray();
					DatagramPacket packet = new DatagramPacket(buf, buf.length, address, 123);
					NtpMessage.encodeTimestamp(packet.getData(), 40, (System.currentTimeMillis()/1000.0) + 2208988800.0);
					socket.send(packet);
					
					System.out.println("NTP request sent, waiting for response...\n");
					packet = new DatagramPacket(buf, buf.length);
					socket.receive(packet);
					double destinationTimestamp = (System.currentTimeMillis()/1000.0) + 2208988800.0;
					
					NtpMessage msg = new NtpMessage(packet.getData());
					double offset = ((msg.receiveTimestamp - msg.originateTimestamp) + (msg.transmitTimestamp - destinationTimestamp)) / 2;
					
					socket.close();
					publishProgress(offset);
					
				} catch(UnknownHostException e) {
					Toast.makeText(getApplicationContext(), "Unknown host: " + e, Toast.LENGTH_SHORT);
				} catch(IOException e) {
					Toast.makeText(getApplicationContext(), "IO error " + e, Toast.LENGTH_SHORT);
				};

				try {
					Thread.sleep(10 * 1000);
				} catch(InterruptedException e) {
				};
			}

			return 0;
		}


		@Override
		protected void onProgressUpdate(Double... val)
		{
			clock_offset = val[0];
			//Log.i("fwloc", "Local clock offset: " + clock_offset);
			send_event(String.format("e:ntp o:%.3f\n", clock_offset));
		}
	}



	/*
	 * Audio task
	 */

	private class AudioTask extends AsyncTask<Void, Double, Integer> 
	{
	
		@Override
		protected Integer doInBackground(Void... params) 
		{
			buffer = new short[buflen];
			int len = 0;

			while(! isCancelled() && rec != null) {

				len = rec.read(buffer, 0, buflen);
				double t = System.currentTimeMillis() / 1000.0 + clock_offset;
				
				/* For each 20 msec of audio, low pass filter
				 * audio and calculate peak value */

				int i, j;
				int s = srate / 50; /* ~20 msec */
						
				for(i=0; i<len; i+=s) {
					double max = 0;
					for(j=0; j<s; j++) {
						a = a * 0.5 + buffer[i+j] * 0.5;
						max = Math.max(Math.abs(a), max);
					}
					publishProgress(max, t);
					t += (double)s / srate;
				}
			}

			return 0;
		}


		@Override
		protected void onProgressUpdate(Double... val)
		{
			double max = val[0];
			double t = val[1];
			double v = Math.log(max);

			if(vavg == 0) {
				vavg = v;
			} else {
				vavg = vavg * 0.95 + v * 0.05;
			}

			paint.setColor(0xFF000000);
			canvas.drawLine(x, 0, x, h, paint);
	
			paint.setColor(0xFF00FF00);
			int y = (int)( (h / 3) * (v - vavg * 0.8) / (Math.log(32767) - vavg));
			canvas.drawLine(x, h/2-y, x, h/2+y, paint);

			if(v > pv + 1) {
				if(sock != null) {
					double lat = location.getLatitude();
					double lon = location.getLongitude();
					String msg = String.format("e:boom v:%.3f t:%.3f l:%f,%f\n", v, t, lon, lat);
					send_event(msg);
					Log.d("fwloc", msg);
				}

				paint.setColor(0xFFFF0000);
				canvas.drawLine(x, 0, x, h, paint);
			}
			
			x = (x + 1) % (w-1);
			
			paint.setColor(0xFFFFFFFF);
			canvas.drawLine(x, 0, x, h, paint);

			if(gv != null) gv.invalidate();
			py = y;
			pv = v;
		}
	}

	/* 
	 * Location handlers
	 */

	@Override
	public void onLocationChanged(Location new_location) 
	{
		location = new_location;
		if (location != null) {
			double lat = location.getLatitude();
			double lon = location.getLongitude();
			send_event(String.format("e:location l:%f,%f\n", lon, lat));
		}
	}


	@Override
	public void onProviderEnabled(String provider) 
	{
	}


	@Override
	public void onProviderDisabled(String provider) 
	{
	}


	@Override
	public void onStatusChanged(String provider, int status, Bundle extras) 
	{
	}

	/*
	 * Application handlers
	 */

	@Override
	public void onCreate(Bundle saved)
	{
		//Log.i("fwloc", "onCreate");
		super.onCreate(saved);

		requestWindowFeature(Window.FEATURE_NO_TITLE);
			
		gv = new GraphView(this);
		setContentView(gv);

		try {
			sock = new DatagramSocket();
		} catch(SocketException e) { 
			Log.w("fwlock", "Could not open socket");
		};

		canvas.drawColor(Color.BLACK);
	}


	@Override
	public void onDestroy()
	{
		//Log.i("fwloc", "onDestroy");
		super.onDestroy();
		sock.close();
	}


	@Override
	public void onStart()
	{
		//Log.i("fwloc", "onStart");
		super.onStart();

		/* Init location */

		locationManager = (LocationManager) getSystemService(Context.LOCATION_SERVICE);
		Criteria criteria = new Criteria();
		String provider = locationManager.getBestProvider(criteria, false);
		location = locationManager.getLastKnownLocation(provider);
		locationManager.requestLocationUpdates(provider, 400, 1, this);
		onLocationChanged(location);
		
		/* Init audio */

		int channelConfiguration = AudioFormat.CHANNEL_CONFIGURATION_MONO;
		int audioEncoding = AudioFormat.ENCODING_PCM_16BIT; 

		buflen = AudioRecord.getMinBufferSize(srate, channelConfiguration, audioEncoding);

		rec = new AudioRecord(
				android.media.MediaRecorder.AudioSource.MIC,
				srate, 
				channelConfiguration,
				audioEncoding,
				buflen);

		rec.startRecording();

		audiotask = new AudioTask();
		audiotask.execute();
	
		/* Init NTP */

		ntptask = new NtpTask();
		ntptask.execute();

		device_id = Settings.Secure.getString(getContentResolver(),Settings.Secure.ANDROID_ID);
	}


	@Override
	public void onStop()
	{
		//Log.i("fwloc", "onStop");
		super.onStop();

		audiotask.cancel(true);
		ntptask.cancel(true);

		rec.stop(); 
		rec.release();
		rec = null;
	}
}

/* End */

