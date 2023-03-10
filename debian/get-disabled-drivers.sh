disable_drivers () {
	enabled_drvs=$1
	types=$(ls -d drivers/*/)
	for t in $types; do
		drivers=$(ls -d "$t"*/)
		for d in $drivers; do
			enable="False"
			OIFS=$IFS; IFS=','
			for e in $enabled_drvs; do
				if [ -z "${d##*"$e"*}" ]; then enable="True"; fi
			done
			IFS=$OIFS
			if [ "$enable" = "False" ]; then
				disabled="$(printf '%s' "$d" | cut -d'/' -f2-),$disabled"
			fi
		done
	done
	printf '%s' "$disabled"
}
