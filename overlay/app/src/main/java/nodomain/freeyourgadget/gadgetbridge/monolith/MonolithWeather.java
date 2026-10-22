package nodomain.freeyourgadget.gadgetbridge.monolith;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import nodomain.freeyourgadget.gadgetbridge.GBApplication;

/**
 * Air temperature for the dashboard and the watch, straight from Open-Meteo (no account, no key)
 * at the phone's position, so no separate weather app is needed.
 */
final class MonolithWeather {
    private static final String PREFS = "monolith_weather";
    private static final long MAX_AGE_MS = 30 * 60 * 1000L;

    static final class Reading {
        final double celsius;
        final double maxCelsius;
        final double minCelsius;
        final int wmoCode;
        final int humidity;
        final double lat;
        final double lon;

        Reading(double celsius, double maxCelsius, double minCelsius, int wmoCode, int humidity, double lat, double lon) {
            this.celsius = celsius;
            this.maxCelsius = maxCelsius;
            this.minCelsius = minCelsius;
            this.wmoCode = wmoCode;
            this.humidity = humidity;
            this.lat = lat;
            this.lon = lon;
        }
    }

    private MonolithWeather() {
    }

    static Reading cached(Context context) {
        SharedPreferences p = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        if (!p.contains("t")) {
            return null;
        }
        return new Reading(p.getFloat("t", 0), p.getFloat("max", 0), p.getFloat("min", 0),
                p.getInt("code", 0), p.getInt("hum", 0), p.getFloat("lat", 0), p.getFloat("lon", 0));
    }

    static boolean stale(Context context) {
        long at = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getLong("at", 0);
        return System.currentTimeMillis() - at > MAX_AGE_MS;
    }

    /** Blocking network call; returns null when offline. */
    static Reading fetch(Context context, double lat, double lon) {
        String url = String.format(Locale.US,
                "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
                        + "&current=temperature_2m,relative_humidity_2m,weather_code"
                        + "&daily=temperature_2m_max,temperature_2m_min&forecast_days=1&timezone=auto", lat, lon);
        HttpURLConnection conn = null;
        try {
            conn = (HttpURLConnection) new URL(url).openConnection();
            conn.setConnectTimeout(10_000);
            conn.setReadTimeout(10_000);
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            try (InputStream in = conn.getInputStream()) {
                byte[] buf = new byte[4096];
                for (int n; (n = in.read(buf)) > 0; ) {
                    out.write(buf, 0, n);
                }
            }
            JSONObject json = new JSONObject(out.toString("UTF-8"));
            JSONObject current = json.getJSONObject("current");
            JSONObject daily = json.getJSONObject("daily");
            Reading r = new Reading(current.getDouble("temperature_2m"),
                    daily.getJSONArray("temperature_2m_max").getDouble(0),
                    daily.getJSONArray("temperature_2m_min").getDouble(0),
                    current.optInt("weather_code", 0), current.optInt("relative_humidity_2m", 0), lat, lon);
            context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
                    .putFloat("t", (float) r.celsius).putFloat("max", (float) r.maxCelsius).putFloat("min", (float) r.minCelsius)
                    .putInt("code", r.wmoCode).putInt("hum", r.humidity)
                    .putFloat("lat", (float) lat).putFloat("lon", (float) lon)
                    .putLong("at", System.currentTimeMillis()).apply();
            return r;
        } catch (Exception e) {
            Log.w("Monolith", "weather fetch failed", e);
            return null;
        } finally {
            if (conn != null) {
                conn.disconnect();
            }
        }
    }

    /**
     * Hands the reading to the connected watch as upstream weather. Reflective: the
     * WeatherSpec class and its fields keep moving between upstream releases.
     */
    static void sendToWatch(Reading r) {
        try {
            Class<?> specClass = null;
            for (String name : new String[]{
                    "nodomain.freeyourgadget.gadgetbridge.model.weather.WeatherSpec",
                    "nodomain.freeyourgadget.gadgetbridge.model.WeatherSpec"}) {
                try {
                    specClass = Class.forName(name);
                    break;
                } catch (ClassNotFoundException e) {
                    // Try the next known location.
                }
            }
            if (specClass == null) {
                return;
            }
            Object spec = specClass.getConstructor().newInstance();
            set(spec, "timestamp", (int) (System.currentTimeMillis() / 1000));
            set(spec, "location", "");
            set(spec, "currentTemp", kelvin(r.celsius));
            set(spec, "todayMaxTemp", kelvin(r.maxCelsius));
            set(spec, "todayMinTemp", kelvin(r.minCelsius));
            set(spec, "currentHumidity", r.humidity);
            set(spec, "currentConditionCode", owmCode(r.wmoCode));
            set(spec, "currentCondition", condition(r.wmoCode));
            set(spec, "latitude", (float) r.lat);
            set(spec, "longitude", (float) r.lon);

            ArrayList<Object> specs = new ArrayList<>();
            specs.add(spec);
            for (String holder : new String[]{
                    "nodomain.freeyourgadget.gadgetbridge.model.weather.Weather",
                    "nodomain.freeyourgadget.gadgetbridge.model.Weather"}) {
                try {
                    Class<?> c = Class.forName(holder);
                    Object instance;
                    try {
                        instance = c.getMethod("getInstance").invoke(null);
                    } catch (NoSuchMethodException e) {
                        instance = c.getField("INSTANCE").get(null);
                    }
                    c.getMethod("setWeatherSpec", List.class).invoke(instance, specs);
                    break;
                } catch (Exception e) {
                    // Try the next known location.
                }
            }
            Object service = GBApplication.deviceService();
            for (Method m : service.getClass().getMethods()) {
                if (m.getName().equals("onSendWeather") && m.getParameterTypes().length == 1) {
                    m.invoke(service, m.getParameterTypes()[0].isAssignableFrom(ArrayList.class) ? specs : spec);
                    return;
                }
            }
        } catch (Exception e) {
            Log.w("Monolith", "weather to watch failed", e);
        }
    }

    private static void set(Object target, String name, Object value) {
        String setter = "set" + Character.toUpperCase(name.charAt(0)) + name.substring(1);
        for (Method m : target.getClass().getMethods()) {
            if (m.getName().equals(setter) && m.getParameterTypes().length == 1) {
                try {
                    m.invoke(target, value);
                    return;
                } catch (Exception e) {
                    break;
                }
            }
        }
        try {
            Field f = target.getClass().getDeclaredField(name);
            f.setAccessible(true);
            f.set(target, value);
        } catch (Exception e) {
            // Field not present in this upstream version.
        }
    }

    private static int kelvin(double celsius) {
        return (int) Math.round(celsius + 273.15);
    }

    /** WMO weather code (Open-Meteo) to the OpenWeatherMap code upstream expects. */
    private static int owmCode(int wmo) {
        if (wmo == 0) return 800;
        if (wmo == 1) return 801;
        if (wmo == 2) return 802;
        if (wmo == 3) return 804;
        if (wmo == 45 || wmo == 48) return 741;
        if (wmo >= 51 && wmo <= 57) return 300;
        if (wmo == 61 || wmo == 80) return 500;
        if (wmo == 63 || wmo == 81) return 501;
        if (wmo == 65 || wmo == 82) return 502;
        if (wmo == 66 || wmo == 67) return 511;
        if (wmo >= 71 && wmo <= 77) return wmo == 75 ? 602 : 600;
        if (wmo == 85 || wmo == 86) return 621;
        if (wmo >= 95) return 211;
        return 3200;
    }

    private static String condition(int wmo) {
        if (wmo == 0) return "Ясно";
        if (wmo <= 2) return "Переменная облачность";
        if (wmo == 3) return "Пасмурно";
        if (wmo == 45 || wmo == 48) return "Туман";
        if (wmo >= 51 && wmo <= 67) return "Дождь";
        if (wmo >= 71 && wmo <= 77) return "Снег";
        if (wmo >= 80 && wmo <= 82) return "Ливень";
        if (wmo == 85 || wmo == 86) return "Снегопад";
        if (wmo >= 95) return "Гроза";
        return "";
    }
}
