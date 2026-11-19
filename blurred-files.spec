%post
/usr/bin/gtk-update-icon-cache -f -t %{_datadir}/icons/hicolor &> /dev/null || :
/usr/bin/update-desktop-database %{_datadir}/applications &> /dev/null || :

%postun
/usr/bin/gtk-update-icon-cache -f -t %{_datadir}/icons/hicolor &> /dev/null || :
/usr/bin/update-desktop-database %{_datadir}/applications &> /dev/null || :

%files
%{_bindir}/blurred_files
%{_datadir}/blurred_files/style.css
%{_datadir}/icons/hicolor/scalable/apps/blurred_files.svg
%{_datadir}/applications/blurred_files.desktop